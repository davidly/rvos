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
#include <vector>

#include <djltrace.hxx>
#include <djl_perf.hxx>

#include "riscv.hxx"

CDJLTrace tracer;
vector<uint8_t> memory;                        // RAM for the vm
uint64_t base_address = 0;                     // vm address of start of memory
uint64_t execution_address = 0;                // where the program counter starts
bool compressed_rvc = false;                   // is the app compressed risc-v?
const uint64_t stack_reservation = 64 * 1024;  // RAM to allocate for the fixed stack
bool g_terminate = false;                      // the app asked to shut down
int exit_code = 0;                             // exit code of the app in the vm

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

// this is called when the risc-v app has an ecall instruction

void riscv_invoke_ecall( RiscV & cpu )
{
    tracer.Trace( "invoke_ecall a7 %llx, a0 %llx, a1 %llx\n", cpu.regs[ RiscV::a7 ], cpu.regs[ RiscV::a0 ], cpu.regs[ RiscV::a1 ] );

    switch ( cpu.regs[ RiscV::a7 ] )
    {
        case 1: // exit
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
        default:
        {
            printf( "error; ecall invoked with unknown command %lld\n", cpu.regs[ RiscV::a7 ] );
        }
    }
} //riscv_invoke_ecall

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
    }

    if ( 0 == base_address )
        usage( "base address of elf image is invalid; physical address required" );

    // allocate space at the end of RAM for the stack

    memory_size += stack_reservation;
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

    tracer.Trace( "vm base address %llx\n", base_address );
    tracer.Trace( "memory size: %llx\n", memory_size );
    tracer.Trace( "stack size reserved: %llx\n", stack_reservation );
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
    }

    if ( 0 == base_address )
        printf( "base address of elf image is zero; physical address required for the rvos emulator\n" );

    printf( "contains 2-byte compressed RVC instructions: %s\n", compressed_rvc ? "yes" : "no" );
    printf( "vm base address %llx\n", base_address );
    printf( "memory size: %llx\n", memory_size );
    printf( "stack size reserved: %llx\n", stack_reservation );
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
        RiscV cpu( memory, base_address, execution_address, compressed_rvc, stack_reservation );
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
        }
    }

    tracer.Shutdown();

    return exit_code;
} //main
