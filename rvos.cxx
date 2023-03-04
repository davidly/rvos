/*
    This emulates an extemely simple RISC-V OS.
    It can load and execute 64-bit RISC-V apps build with Gnu tools in .elf files.
    The OS supports 2 ABI: exit and print string
    Written by David Lee in February 2023
*/

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <sys\stat.h>
#include <vector>
#include <chrono>

#include <djltrace.hxx>
#include <djl_perf.hxx>

#include "riscv.hxx"

CDJLTrace tracer;
bool compressed_rvc = false;                   // is the app compressed risc-v?
const uint64_t stack_commit = 64 * 1024;       // RAM to allocate for the fixed stack
const uint64_t brk_commit = 1024 * 1024;       // RAM to reserve if the app calls brk to allocate space
bool g_terminate = false;                      // the app asked to shut down
int exit_code = 0;                             // exit code of the app in the vm
vector<uint8_t> memory;                        // RAM for the vm
uint64_t base_address = 0;                     // vm address of start of memory
uint64_t execution_address = 0;                // where the program counter starts
uint64_t brk_address = 0;                      // offset of brk, initially end_of_data
uint64_t end_of_data = 0;                      // official end of the loaded app
uint64_t bottom_of_stack = 0;                  // just beyond where brk might move

#pragma pack( push, 1 )

struct ElfHeader64
{
    uint32_t magic;
    uint8_t bit_width;
    uint8_t endianness;
    uint8_t elf_version;
    uint8_t os_abi;
    uint8_t os_avi_version;
    uint8_t padding[ 7 ];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry_point;
    uint64_t program_header_table;
    uint64_t section_header_table;
    uint32_t flags;
    uint16_t header_size;
    uint16_t program_header_table_size;
    uint16_t program_header_table_entries;
    uint16_t section_header_table_size;
    uint16_t section_header_table_entries;
    uint16_t section_with_section_names;
};

struct ElfSymbol64
{
    uint32_t name;          // index into symbol string table
    uint8_t info;           // value of the symbol
    uint8_t other;
    uint16_t shndx;
    uint64_t value;
    uint64_t size;

    const char * show_info()
    {
        if ( 0 == info )
            return "local";
        if ( 1 == info )
            return "global";
        if ( 2 == info )
            return "weak";
        if ( 3 == info )
            return "num";
        if ( 4 == info )
            return "file";
        if ( 5 == info )
            return "common";
        if ( 6 == info )
            return "tls";
        if ( 7 == info )
            return "num";
        if ( 10 == info )
            return "loos / gnu_ifunc";
        if ( 12 == info )
            return "hios";
        if ( 13 == info )
            return "loproc";
        if ( 15 == info )
            return "hiproc";
        return "unknown";
    } //show_info

    const char * show_other()
    {
        if ( 0 == other )
            return "default";
        if ( 1 == other )
            return "internal";
        if ( 2 == other )
            return "hidden";
        if ( 3 == other )
            return "protected";
        return "unknown";
    }
};

struct ElfProgramHeader64
{
    uint32_t type;
    uint32_t flags;
    uint64_t offset_in_image;
    uint64_t virtual_address;
    uint64_t physical_address;
    uint64_t file_size;
    uint64_t mem_size;
    uint64_t alignment;

    const char * show_type()
    {
        if ( 0 == type )
            return "unused";
        if ( 1 == type )
            return "load";
        return "unknown";
    }
};

struct ElfSectionHeader64
{
    uint32_t name_offset;
    uint32_t type;
    uint64_t flags;
    uint64_t address;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t address_alignment;
    uint64_t entry_size;

    const char * show_type()
    {
        if ( 0 == type )
            return "unused";
        if ( 1 == type )
            return "program data";
        if ( 2 == type )
            return "symbol table";
        if ( 3 == type )
            return "string table";
        if ( 4 == type )
            return "relocation entries";
        if ( 5 == type )
            return "symbol hash table";
        return "unknown";
    }

    const char * show_flags()
    {
        static char ac[ 80 ];
        ac[0] = 0;

        if ( flags & 0x1 )
            strcat( ac, "write, " );
        if ( flags & 0x2 )
            strcat( ac, "alloc, " );
        if ( flags & 0x4 )
            strcat( ac, "executable, " );
        if ( flags & 0x10 )
            strcat( ac, "merge, " );
        if ( flags & 0x20 )
            strcat( ac, "asciz strings, " );
        return ac;
    }
};

#pragma pack(pop)

class CFile
{
    public: 
        FILE *fp;
        CFile( FILE * f ) : fp( f ) {}
        ~CFile() { fclose( fp ); }
};

void usage( char const * perror = 0 )
{
    if ( 0 != perror )
        printf( "error: %s\n", perror );

    printf( "usage: rvos <elf_executable>\n" );
    printf( "   arguments:    -t     enable debug tracing to rvos.log\n" );
    printf( "                 -i     if -t is set, also enables risc-v instruction tracing\n" );
    printf( "                 -e     just show information about the elf executable; don't actually run it\n" );
    printf( "                 -p     shows performance information at app exit\n" );
    exit( 1 );
} //usage

struct linux_timeval
{
    int64_t tv_sec;
    int64_t tv_usec;
};

int gettimeofday( linux_timeval * tp, struct timezone* tzp )
{
    namespace sc = std::chrono;
    sc::system_clock::duration d = sc::system_clock::now().time_since_epoch();
    sc::seconds s = sc::duration_cast<sc::seconds>(d);
    tp->tv_sec = s.count();
    tp->tv_usec = sc::duration_cast<sc::microseconds>(d - s).count();

    return 0;
} //gettimeofday

uint64_t rand64()
{
    uint64_t r = 0;

    for ( int i = 0; i < 7; i++ )
        r = ( r << 15 ) | ( rand() & 0x7FFF );

    return r;
} //rand64

// this is called when the risc-v app has an ecall instruction
// if i get ambitious...

#define SYS_getcwd 17
#define SYS_dup 23
#define SYS_fcntl 25
#define SYS_faccessat 48
#define SYS_chdir 49
#define SYS_openat 56
#define SYS_close 57
#define SYS_getdents 61
#define SYS_lseek 62
#define SYS_read 63
#define SYS_write 64
#define SYS_writev 66
#define SYS_pread 67
#define SYS_pwrite 68
#define SYS_fstatat 79
#define SYS_fstat 80
#define SYS_exit 93
#define SYS_exit_group 94
#define SYS_kill 129
#define SYS_rt_sigaction 134
#define SYS_times 153
#define SYS_uname 160
#define SYS_gettimeofday 169
#define SYS_getpid 172
#define SYS_getuid 174
#define SYS_geteuid 175
#define SYS_getgid 176
#define SYS_getegid 177
#define SYS_brk 214
#define SYS_munmap 215
#define SYS_mremap 216
#define SYS_mmap 222
#define SYS_open 1024
#define SYS_link 1025
#define SYS_unlink 1026
#define SYS_mkdir 1030
#define SYS_access 1033
#define SYS_stat 1038
#define SYS_lstat 1039
#define SYS_time 1062
#define SYS_getmainvars 2011

void riscv_invoke_ecall( RiscV & cpu )
{
    tracer.Trace( "invoke_ecall a7 %llx, a0 %llx, a1 %llx, a2 %llx, a3 %llx\n", cpu.regs[ RiscV::a7 ],
                  cpu.regs[ RiscV::a0 ], cpu.regs[ RiscV::a1 ], cpu.regs[ RiscV::a2 ], cpu.regs[ RiscV::a3 ] );

    switch ( cpu.regs[ RiscV::a7 ] )
    {
        case 1: // exit
        case SYS_exit:
        {
            tracer.Trace( "  rvos command 1: exit app\n" );
            g_terminate = true;
            cpu.end_emulation();
            exit_code = (int) cpu.regs[ RiscV::a0 ];
            break;
        }
        case 2: // print asciiz in a0
        {
            tracer.Trace( "  rvos command 2: print string '%s'\n", (char *) cpu.getmem( cpu.regs[ RiscV::a0 ] ) );
            printf( "%s", (char *) cpu.getmem( cpu.regs[ RiscV::a0 ] ) );
            break;
        }
        case SYS_fstat:
        {
            tracer.Trace( "  rvos command SYS_fstat\n" );
            struct _stat64 *pstat = (struct _stat64 *) cpu.getmem( cpu.regs[ RiscV::a1 ] );
            if ( 1 == cpu.regs[ RiscV::a0 ] ) // 1 == stdout
            {
                pstat->st_mode = S_IFCHR;
                pstat->st_ino = 3;
                pstat->st_mode = 0x2190;
                pstat->st_nlink = 1;
                pstat->st_uid = 1000;
                pstat->st_gid = 5;
                pstat->st_rdev = 1024; // this is st_blksize on linux
                pstat->st_size = 0;
                //pstat->st_blocks = 0; // this field doesn't exist in Windows.
            }
            break;
        }
        case SYS_gettimeofday:
        {
            tracer.Trace( "  rvos command SYS_gettimeofday\n" );
            linux_timeval * ptimeval = (linux_timeval *) cpu.getmem( cpu.regs[ RiscV::a0 ] );
            int result = 0;
            if ( 0 != ptimeval )
                result = gettimeofday( ptimeval, 0 );
            cpu.regs[ RiscV::a0 ] = result;
            break;
        }
        case SYS_write:
        {
            tracer.Trace( "  rvos command SYS_write\n" );

            uint64_t handle = cpu.regs[ RiscV::a0 ];
            uint8_t * p = (uint8_t *) cpu.getmem( cpu.regs[ RiscV::a1 ] );
            uint64_t count = cpu.regs[ RiscV::a2 ];

            if ( 1 == handle || 2 == handle ) // stdout / stderr
            {
                tracer.Trace( "  writing '%.*s'\n", (int) count, p );
                printf( "%.*s", (int) count, p );
                cpu.regs[ RiscV::a0 ] = count;
            }
            break;
        }
        case SYS_close:
        {
            tracer.Trace( "  rvos command SYS_close\n" );

            uint64_t handle = cpu.regs[ RiscV::a0 ];
            if ( handle >=0 && handle <= 3 )
            {
                // built-in handle stdin, stdout, stderr -- ignore
            }
            else
            {
            }
            break;
        }
        case SYS_brk:
        {
            uint64_t original = brk_address;
            uint64_t ask = cpu.regs[ RiscV::a0 ];
            if ( 0 == ask )
                cpu.regs[ RiscV::a0 ] = cpu.get_vm_address( brk_address );
            else
            {
                uint64_t ask_offset = ask - base_address;

                tracer.Trace( "ask_offset %llx, end_of_data %llx, end_of_stack %llx\n", ask_offset, end_of_data, bottom_of_stack );

                if ( ask_offset >= end_of_data && ask_offset < bottom_of_stack )
                    brk_address = cpu.getoffset( ask );
                else
                {
                    tracer.Trace( "  allocation request was too large, failing it by returning current brk\n" );
                    cpu.regs[ RiscV::a0 ] = cpu.get_vm_address( brk_address );
                }
            }

            tracer.Trace( "  SYS_brk. ask %llx, current brk %llx, new brk %llx, result in a0 %llx\n", ask, original, brk_address, cpu.regs[ RiscV::a0 ] );
            break;
        }
        case 0x2000: // rand64. returns an unsigned random number in a0
        {
            cpu.regs[ RiscV::a0 ] = rand64();
            break;
        }
        case 0x2001: // print_double in a0
        {
            double d;
            memcpy( &d, &cpu.regs[ RiscV::a0 ], sizeof d );
            printf( "%lf", d );
            break;
        }
        default:
        {
            printf( "error; ecall invoked with unknown command %lld, a0 %#llx, a1 %#llx, a2 %#llx\n",
                    cpu.regs[ RiscV::a7 ], cpu.regs[ RiscV::a0 ], cpu.regs[ RiscV::a1 ], cpu.regs[ RiscV::a2 ] );
        }
    }
} //riscv_invoke_ecall

vector<char> g_string_table;                   // strings in the elf image
vector<ElfSymbol64> g_symbols;                 // symbols in the elf image

// returns the best guess for a symbol name for the address

const char * riscv_symbol_lookup( RiscV & cpu, uint64_t address )
{
    if ( address < base_address || address > ( base_address + memory.size() ) )
        return "";

    for ( size_t i = 0; i < g_symbols.size(); i++ )
    {
        if ( ( address >= g_symbols[ i ].value ) &&
             ( ( i == ( g_symbols.size() - 1 ) ) || ( address < g_symbols[ i + 1 ].value ) ) )
            return & g_string_table[ g_symbols[ i ].name ];
    }

    return "";
} //riscv_symbol_lookup

int symbol_compare( const void * a, const void * b )
{
    ElfSymbol64 * pa = (ElfSymbol64 *) a;
    ElfSymbol64 * pb = (ElfSymbol64 *) b;

    if ( pa->value > pb->value )
        return 1;
    if ( pa->value == pb->value )
        return 0;
    return -1;
} //symbol_compare

bool load_image( const char * pimage )
{
    ElfHeader64 ehead = {0};

    FILE * fp = fopen( pimage, "rb" );
    if ( !fp )
        usage( "can't open image file" );

    CFile file( fp );
    size_t read = fread( &ehead, sizeof ehead, 1, fp );

    if ( 1 != read )
        usage( "image file is invalid" );

    if ( 0x464c457f != ehead.magic )
        usage( "image file's magic header is invalid" );

    if ( 0xf3 != ehead.machine )
        usage( "image isn't for RISC-V" );

    tracer.Trace( "header fields:\n" );
    tracer.Trace( "  entry address: %llx\n", ehead.entry_point );
    tracer.Trace( "  program entries: %u\n", ehead.program_header_table_entries );
    tracer.Trace( "  program header entry size: %u\n", ehead.program_header_table_size );
    tracer.Trace( "  program offset: %llu == %llx\n", ehead.program_header_table, ehead.program_header_table );
    tracer.Trace( "  section entries: %u\n", ehead.section_header_table_entries );
    tracer.Trace( "  section header entry size: %u\n", ehead.section_header_table_size );
    tracer.Trace( "  section offset: %llu == %llx\n", ehead.section_header_table, ehead.section_header_table );
    tracer.Trace( "  flags: %x\n", ehead.flags );

    execution_address = ehead.entry_point;
    compressed_rvc = 0 != ( ehead.flags & 1 ); // 2-byte compressed RVC instructions, not 4-byte default risc-v instructions

    // determine how much RAM to allocate

    uint64_t memory_size = 0;

    for ( uint16_t ph = 0; ph < ehead.program_header_table_entries; ph++ )
    {
        size_t o = ehead.program_header_table + ( ph * ehead.program_header_table_size );
        tracer.Trace( "program header %u at offset %zu\n", ph, o );

        ElfProgramHeader64 head = {0};

        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, __min( sizeof( head ), ehead.program_header_table_size ), 1, fp );
        if ( 1 != read )
            usage( "can't read program header" );

        tracer.Trace( "  type: %u / %s\n", head.type, head.show_type() );
        tracer.Trace( "  offset in image: %llx\n", head.offset_in_image );
        tracer.Trace( "  virtual address: %llx\n", head.virtual_address );
        tracer.Trace( "  physical address: %llx\n", head.physical_address );
        tracer.Trace( "  file size: %llx\n", head.file_size );
        tracer.Trace( "  memory size: %llx\n", head.mem_size );
        tracer.Trace( "  alignment: %llx\n", head.alignment );

        memory_size += head.mem_size;

        if ( 0 == ph )
            base_address = head.physical_address;
    }

    // first load the string table

    for ( uint16_t sh = 0; sh < ehead.section_header_table_entries; sh++ )
    {
        size_t o = ehead.section_header_table + ( sh * ehead.section_header_table_size );
        ElfSectionHeader64 head = {0};

        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, __min( sizeof( head ), ehead.section_header_table_size ), 1, fp );
        if ( 1 != read )
            usage( "can't read section header" );

        if ( 3 == head.type )
        {
            g_string_table.resize( head.size );
            fseek( fp, (long) head.offset, SEEK_SET );
            read = fread( g_string_table.data(), head.size, 1, fp );
            if ( 1 != read )
                usage( "can't read string table\n" );

            break;
        }
    }

    for ( uint16_t sh = 0; sh < ehead.section_header_table_entries; sh++ )
    {
        size_t o = ehead.section_header_table + ( sh * ehead.section_header_table_size );
        tracer.Trace( "section header %u at offset %zu == %zx\n", sh, o, o );

        ElfSectionHeader64 head = {0};

        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, __min( sizeof( head ), ehead.section_header_table_size ), 1, fp );
        if ( 1 != read )
            usage( "can't read section header" );

        tracer.Trace( "  type: %u / %s\n", head.type, head.show_type() );
        tracer.Trace( "  flags: %llx / %s\n", head.flags, head.show_flags() );
        tracer.Trace( "  address: %llx\n", head.address );
        tracer.Trace( "  offset: %llx\n", head.offset );
        tracer.Trace( "  size: %llx\n", head.size );

        if ( 2 == head.type )
        {
            g_symbols.resize( head.size / sizeof( ElfSymbol64 ) );
            fseek( fp, (long) head.offset, SEEK_SET );
            read = fread( g_symbols.data(), head.size, 1, fp );
            if ( 1 != read )
                usage( "can't read symbol table\n" );
        }
    }

    tracer.Trace( "elf image has %zd symbols\n", g_symbols.size() );

    // void out the entries that don't have symbol names

    for ( size_t se = 0; se < g_symbols.size(); se++ )
        if ( 0 == g_symbols[se].name )
            g_symbols[se].value = 0;

    qsort( g_symbols.data(), g_symbols.size(), sizeof( ElfSymbol64 ), symbol_compare );

    // remove symbols that don't look like addresses

    while ( g_symbols.size() && ( g_symbols[ 0 ].value < base_address ) )
        g_symbols.erase( g_symbols.begin() );

    for ( size_t se = 0; se < g_symbols.size(); se++ )
        tracer.Trace( "    symbol %llx == %s\n", g_symbols[se].value, & g_string_table[ g_symbols[ se ].name ] );

    if ( 0 == base_address )
        usage( "base address of elf image is invalid; physical address required" );

    // allocate space after uninitialized memory brk to request space and the stack at the end
    // memory map:
    //     base_address (offset read from the .elf file)
    //     code (read from the .elf file)
    //     initialized data (read from the .elf file)
    //     uninitalized data (size read from the .elf file)
    //     end_of_data
    //     brk_address with uninitialized RAM (same as end_of_data initially)
    //     (unallocated space between brk and the bottom of the stack)
    //     bottom_of_stack
    //     starting stack / one byte beyond RAM for the app

    end_of_data = memory_size;
    brk_address = memory_size;
    memory_size += brk_commit;
    bottom_of_stack = memory_size;
    memory_size += stack_commit;
    memory.resize( memory_size );
    memset( memory.data(), 0, memory_size );

    // load the program into RAM

    for ( uint16_t ph = 0; ph < ehead.program_header_table_entries; ph++ )
    {
        size_t o = ehead.program_header_table + ( ph * ehead.program_header_table_size );
        ElfProgramHeader64 head = {0};
        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, __min( sizeof( head ), ehead.program_header_table_size ), 1, fp );

        if ( 0 != head.file_size )
        {
            fseek( fp, (long) head.offset_in_image, SEEK_SET );
            fread( memory.data() + head.physical_address - base_address, head.file_size, 1, fp );
        }
    }

    tracer.Trace( "risc-v compressed instructions: %d\n", compressed_rvc );
    tracer.Trace( "vm base address %llx\n", base_address );
    tracer.Trace( "memory size: %llx\n", memory_size );
    tracer.Trace( "brk size reserved: %llx\n", brk_commit );
    tracer.Trace( "stack size reserved: %llx\n", stack_commit );
    tracer.Trace( "initial brk offset: %llx\n", brk_address );
    tracer.Trace( "end_of_data: %llx\n", end_of_data );
    tracer.Trace( "bottom_of_stack: %llx\n", bottom_of_stack );
    tracer.Trace( "initial sp offset: %llx\n", memory_size );
    tracer.Trace( "execution begins at %llx\n", execution_address );

    return true;
} //load_image

void elf_info( const char * pimage )
{
    ElfHeader64 ehead = {0};

    FILE * fp = fopen( pimage, "rb" );
    if ( !fp )
        usage( "can't open image file" );

    CFile file( fp );
    size_t read = fread( &ehead, sizeof ehead, 1, fp );

    if ( 1 != read )
    {
        printf( "image file is invalid; can't read data\n" );
        return;
    }

    if ( 0x464c457f != ehead.magic )
    {
        printf( "image file's magic header is invalid: %x\n", ehead.magic );
        return;
    }

    if ( 0xf3 != ehead.machine )
        printf( "image isn't for RISC-V; continuing anyway. machine type is %x\n", ehead.machine );

    printf( "header fields:\n" );
    printf( "  entry address: %llx\n", ehead.entry_point );
    printf( "  program entries: %u\n", ehead.program_header_table_entries );
    printf( "  program header entry size: %u\n", ehead.program_header_table_size );
    printf( "  program offset: %llu == %llx\n", ehead.program_header_table, ehead.program_header_table );
    printf( "  section entries: %u\n", ehead.section_header_table_entries );
    printf( "  section header entry size: %u\n", ehead.section_header_table_size );
    printf( "  section offset: %llu == %llx\n", ehead.section_header_table, ehead.section_header_table );
    printf( "  flags: %x\n", ehead.flags );

    execution_address = ehead.entry_point;
    compressed_rvc = 0 != ( ehead.flags & 1 ); // 2-byte compressed RVC instructions, not 4-byte default risc-v instructions

    uint64_t memory_size = 0;

    for ( uint16_t ph = 0; ph < ehead.program_header_table_entries; ph++ )
    {
        size_t o = ehead.program_header_table + ( ph * ehead.program_header_table_size );
        printf( "program header %u at offset %zu\n", ph, o );

        ElfProgramHeader64 head = {0};

        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, __min( sizeof( head ), ehead.program_header_table_size ), 1, fp );
        if ( 1 != read )
            usage( "can't read program header" );

        printf( "  type: %u / %s\n", head.type, head.show_type() );
        printf( "  offset in image: %llx\n", head.offset_in_image );
        printf( "  virtual address: %llx\n", head.virtual_address );
        printf( "  physical address: %llx\n", head.physical_address );
        printf( "  file size: %llx\n", head.file_size );
        printf( "  memory size: %llx\n", head.mem_size );
        printf( "  alignment: %llx\n", head.alignment );

        memory_size += head.mem_size;

        if ( 0 == ph )
            base_address = head.physical_address;
    }

    // first load the string table

    vector<char> string_table;

    for ( uint16_t sh = 0; sh < ehead.section_header_table_entries; sh++ )
    {
        size_t o = ehead.section_header_table + ( sh * ehead.section_header_table_size );
        ElfSectionHeader64 head = {0};

        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, __min( sizeof( head ), ehead.section_header_table_size ), 1, fp );
        if ( 1 != read )
            usage( "can't read section header" );

        if ( 3 == head.type )
        {
            string_table.resize( head.size );
            fseek( fp, (long) head.offset, SEEK_SET );
            read = fread( string_table.data(), head.size, 1, fp );
            if ( 1 != read )
                usage( "can't read string table\n" );

            break;
        }
    }

    for ( uint16_t sh = 0; sh < ehead.section_header_table_entries; sh++ )
    {
        size_t o = ehead.section_header_table + ( sh * ehead.section_header_table_size );
        printf( "section header %u at offset %zu == %zx\n", sh, o, o );

        ElfSectionHeader64 head = {0};

        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, __min( sizeof( head ), ehead.section_header_table_size ), 1, fp );
        if ( 1 != read )
            usage( "can't read section header" );

        printf( "  type: %u / %s\n", head.type, head.show_type() );
        printf( "  flags: %llx / %s\n", head.flags, head.show_flags() );
        printf( "  address: %llx\n", head.address );
        printf( "  offset: %llx\n", head.offset );
        printf( "  size: %llx\n", head.size );

        if ( 2 == head.type )
        {
            vector<ElfSymbol64> symbols;
            symbols.resize( head.size / sizeof( ElfSymbol64 ) );
            fseek( fp, (long) head.offset, SEEK_SET );
            read = fread( symbols.data(), head.size, 1, fp );
            if ( 1 != read )
                usage( "can't read symbol table\n" );

            size_t count = head.size / sizeof( ElfSymbol64 );
            printf( "  symbols:\n" );
            for ( size_t sym = 0; sym < symbols.size(); sym++ )
            {
                printf( "    symbol # %zd\n", sym );
                ElfSymbol64 & sym_entry = symbols[ sym ];
                printf( "     name:  %x == %s\n",   sym_entry.name, ( 0 == sym_entry.name ) ? "" : & string_table[ sym_entry.name ] );
                printf( "     info:  %x == %s\n",   sym_entry.info, sym_entry.show_info() );
                printf( "     other: %x == %s\n",   sym_entry.other, sym_entry.show_other() );
                printf( "     shndx: %x\n",   sym_entry.shndx );
                printf( "     value: %llx\n", sym_entry.value );
                printf( "     size:  %lld\n", sym_entry.size );
            }
        }
    }

    if ( 0 == base_address )
        printf( "base address of elf image is zero; physical address required for the rvos emulator\n" );

    printf( "contains 2-byte compressed RVC instructions: %s\n", compressed_rvc ? "yes" : "no" );
    printf( "vm base address %llx\n", base_address );
    printf( "memory size: %llx\n", memory_size );
    printf( "stack size reserved: %llx\n", stack_commit );
    printf( "execution begins at %llx\n", execution_address );
} //elf_info

int ends_with( const char * str, const char * end )
{
    size_t len = strlen( str );
    size_t lenend = strlen( end );

    if ( len < lenend )
        return false;

    return ( 0 == _stricmp( str + len - lenend, end ) );
} //ends_with

int main( int argc, char * argv[] )
{
    bool trace = false;
    char * pcApp = 0;
    bool showPerformance = false;
    bool traceInstructions = false;
    bool elfInfo = false;
    bool generateRVCTable = false;
    static char acAppArgs[1024] = {0};
    static char acApp[MAX_PATH] = {0};

    for ( int i = 1; i < argc; i++ )
    {
        char *parg = argv[i];
        char c = *parg;

        if ( ( 0 == pcApp ) && ( '-' == c || '/' == c ) )
        {
            char ca = tolower( parg[1] );

            if ( 't' == ca )
                trace = true;
            else if ( 'i' == ca )
                traceInstructions = true;
            else if ( 'g' == ca )
                generateRVCTable = true;
            else if ( 'e' == ca )
                elfInfo = true;
            else if ( 'p' == ca )
                showPerformance = true;
            else
                usage( "invalid argument specified" );
        }
        else
        {
            if ( 0 == pcApp )
                pcApp = parg;
            else if ( strlen( acAppArgs ) + 3 + strlen( parg ) < _countof( acAppArgs ) )
            {
                if ( 0 != acAppArgs[0] )
                    strcat( acAppArgs, " " );

                strcat( acAppArgs, parg );
            }
        }
    }

    tracer.Enable( trace, L"rvos.log", true );
    tracer.SetQuiet( true );

    if ( generateRVCTable )
    {
        bool ok = RiscV::generate_rvc_table( "rvctable.txt" );
        if ( ok )
            printf( "rvctable.txt successfully created\n" );
        else
            printf( "unable to create rvctable.txt\n" );

        return 0;
    }

    if ( 0 == pcApp )
        usage( "no executable specified\n" );

    strcpy( acApp, pcApp );
    if ( !ends_with( acApp, ".elf" ) )
        strcat( acApp, ".elf" );

    if ( elfInfo )
    {
        elf_info( acApp );
        return 0;
    }

    bool ok = load_image( acApp );

    CPerfTime perfApp;

    if ( ok )
    {
        RiscV cpu( memory, base_address, execution_address, compressed_rvc, stack_commit );
        cpu.trace_instructions( traceInstructions );
        uint64_t cycles = 0;

        do
        {
            cycles += cpu.run( 1000 );
        } while( !g_terminate );

        if ( showPerformance )
        {
            LONGLONG elapsed = 0;
            FILETIME creationFT, exitFT, kernelFT, userFT;
            perfApp.CumulateSince( elapsed );
            GetProcessTimes( GetCurrentProcess(), &creationFT, &exitFT, &kernelFT, &userFT );
    
            ULARGE_INTEGER ullK, ullU;
            ullK.HighPart = kernelFT.dwHighDateTime;
            ullK.LowPart = kernelFT.dwLowDateTime;
            ullU.HighPart = userFT.dwHighDateTime;
            ullU.LowPart = userFT.dwLowDateTime;
        
            printf( "kernel CPU ms:    %16ws\n", perfApp.RenderDurationInMS( ullK.QuadPart ) );
            printf( "user CPU ms:      %16ws\n", perfApp.RenderDurationInMS( ullU.QuadPart ) );
            printf( "total CPU ms:     %16ws\n", perfApp.RenderDurationInMS( ullU.QuadPart + ullK.QuadPart ) );
            printf( "elapsed ms:       %16ws\n", perfApp.RenderDurationInMS( elapsed ) );
            printf( "app exit code:    %16d\n", exit_code );
        }
    }

    tracer.Shutdown();

    return exit_code;
} //main
