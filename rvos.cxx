/*
    This emulates an extemely simple RISC-V OS.
    Per https://riscv.org/wp-content/uploads/2017/05/riscv-privileged-v1.10.pdf
    It's an AEE (Application Execution Environment) that exposes an ABI (Application Binary Inferface) for an Application.
    It runs in RISC-V M mode, similar to embedded systems.
    It can load and execute 64-bit RISC-V apps built with Gnu tools in .elf files.
    Written by David Lee in February 2023
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <vector>
#include <chrono>

#include <djl_os.hxx>

#ifdef _MSC_VER
    #include <io.h>

    struct iovec
    {
       void * iov_base; /* Starting address */
       size_t iov_len; /* Length in bytes */
    };

    typedef SSIZE_T ssize_t;
#else
    #include <unistd.h>
    #ifndef OLDGCC      // the several-years-old Gnu C compiler for the RISC-V development boards
        #include <sys/random.h>
        #include <sys/uio.h>
        #ifndef __APPLE__
            #include <sys/sysinfo.h>
        #endif
    #endif
#endif

#include <djltrace.hxx>

using namespace std;
using namespace std::chrono;

#include "riscv.hxx"
#include "rvos.h"

CDJLTrace tracer;
bool g_compressed_rvc = false;                 // is the app compressed risc-v?
const uint64_t g_args_commit = 1024;           // storage spot for command-line arguments
const uint64_t g_stack_commit = 128 * 1024;    // RAM to allocate for the fixed stack
uint64_t g_brk_commit = 1024 * 1024;           // RAM to reserve if the app calls brk to allocate space
bool g_terminate = false;                      // has the app asked to shut down?
int g_exit_code = 0;                           // exit code of the app in the vm
vector<uint8_t> memory;                        // RAM for the vm
uint64_t g_base_address = 0;                   // vm address of start of memory
uint64_t g_execution_address = 0;              // where the program counter starts
uint64_t g_brk_address = 0;                    // offset of brk, initially g_end_of_data
uint64_t g_highwater_brk = 0;                  // highest brk seen during app; peak dynamically-allocated RAM
uint64_t g_end_of_data = 0;                    // official end of the loaded app
uint64_t g_bottom_of_stack = 0;                // just beyond where brk might move
uint64_t g_top_of_stack = 0;                   // argc, argv, penv, aux records sit above this
int * g_perrno = 0;                            // if it exists, it's a pointer to errno for the vm
bool * g_ptracenow = 0;                        // if this variable exists in the vm, it'll control instruction tracing

#pragma pack( push, 1 )

struct AuxProcessStart
{
    uint64_t a_type; // AT_xxx ID from elf.h
    union
    {
        uint64_t a_val;
        void * a_ptr;
        void ( * a_fcn )();
    } a_un;
};

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
    uint32_t name;          // index into the symbol string table
    uint8_t info;           // value of the symbol
    uint8_t other;
    uint16_t shndx;
    uint64_t value;         // address where the symbol resides in memory
    uint64_t size;          // length in memory of the symbol

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
        uint32_t basetype = ( type & 0xf );

        if ( 0 == basetype )
            return "unused";
        if ( 1 == basetype )
            return "load";
        if ( 2 == basetype )
            return "dynamic";
        if ( 3 == basetype )
            return "interp";
        if ( 4 == basetype )
            return "note";
        if ( 5 == basetype )
            return "shlib";
        if ( 6 == basetype )
            return "phdr";
        if ( 7 == basetype )
            return "tls";
        if ( 8 == basetype )
            return "num";
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
        uint32_t basetype = ( type & 0xf );

        if ( 0 == basetype )
            return "unused";
        if ( 1 == basetype )
            return "program data";
        if ( 2 == basetype )
            return "symbol table";
        if ( 3 == basetype )
            return "string table";
        if ( 4 == basetype )
            return "relocation entries";
        if ( 5 == basetype )
            return "symbol hash table";
        if ( 6 == basetype )
            return "dynamic";
        if ( 7 == basetype )
            return "note";
        if ( 8 == basetype )
            return "nobits";
        if ( 9 == basetype )
            return "rel";
        if ( 10 == basetype )
            return "shlib";
        if ( 11 == basetype )
            return "dynsym";
        if ( 12 == basetype )
            return "num";
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
    printf( "   arguments:    -e     just show information about the elf executable; don't actually run it\n" );
    printf( "                 -g     (internal) generate rcvtable.txt then exit\n" );
    printf( "                 -h:X   # of meg for the heap (brk space) 0..1024 are valid. default is 1\n" );
    printf( "                 -i     if -t is set, also enables risc-v instruction tracing\n" );
    printf( "                 -p     shows performance information at app exit\n" );
    printf( "                 -t     enable debug tracing to rvos.log\n" );
    exit( 1 );
} //usage

struct linux_timeval
{
    uint64_t tv_sec;       // time_t
    uint64_t tv_usec;      // suseconds_t
};

int gettimeofday( linux_timeval * tp, struct timezone * tzp )
{
    // the OLDGCC's chrono implementation is built on time(), which calls this but
    // has only second resolution. So the microseconds returned here are lost.
    // All other C++ implementations do the right thing.

    namespace sc = std::chrono;
    sc::system_clock::duration d = sc::system_clock::now().time_since_epoch();
    sc::seconds s = sc::duration_cast<sc::seconds>( d );
    tp->tv_sec = s.count();
    tp->tv_usec = sc::duration_cast<sc::microseconds>( d - s ).count();

    return 0;
} //gettimeofday

uint64_t rand64()
{
    uint64_t r = 0;

    for ( int i = 0; i < 7; i++ )
        r = ( r << 15 ) | ( rand() & 0x7FFF );

    return r;
} //rand64

#ifdef _MSC_VER

int windows_translate_flags( int flags )
{
    // Microsoft C uses different constants for flags than linux:
    // O_CREAT == 0x100 msft, 0x200 linux
    // O_TRUNC == 0x200 msft, 0x400 linux

    if ( flags & 0x200 )
    {
        flags |= 0x100;
        flags &= ~ 0x200;
    }
    if ( flags & 0x400 )
    {
        flags |= 0x200;
        flags &= ~ 0x400;
    }

    flags |= O_BINARY; // this is assumed on Linux systems

    tracer.Trace( "  flags translated for Microsoft: %x\n", flags );
    return flags;
} //windows_translate_flags

void fill_pstat_windows( int descriptor, struct _stat64 * pstat )
{
    if ( descriptor >= 0 && descriptor <= 2 ) // stdin / stdout / stderr
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
    else
    {
        pstat->st_mode = S_IFREG;
        pstat->st_ino = 3;
        pstat->st_mode = 0x2190;
        pstat->st_nlink = 1;
        pstat->st_uid = 1000;
        pstat->st_gid = 5;
        pstat->st_rdev = 4096; // this is st_blksize on linux
        pstat->st_size = 0;
        //pstat->st_blocks = 0; // this field doesn't exist in Windows.
    }
} //fill_pstat_windows

// taken from gnu's time.h.

#define CLOCK_REALTIME             0
#define CLOCK_MONOTONIC            1
#define CLOCK_PROCESS_CPUTIME_ID   2
#define CLOCK_THREAD_CPUTIME_ID    3
#define CLOCK_MONOTONIC_RAW        4

#ifndef CLOCK_REALTIME_COARSE // mingw64's pthread_time.h defines this as 4 because standards
    #define CLOCK_REALTIME_COARSE      5
#endif

#define CLOCK_MONOTONIC_COARSE     6

static const char * clockids[] =
{
    "realtime",
    "monotonic",
    "process_cputime_id",
    "thread_cputime_id",
    "monotonic_raw",
    "realtime_coarse",
    "monotinic_coarse",
};

typedef int clockid_t;

const char * get_clockid( clockid_t clockid )
{
    if ( clockid < _countof( clockids ) )
        return clockids[ clockid ];
    return "unknown";
} //get_clockid

high_resolution_clock::time_point g_tAppStart;

int clock_gettime( clockid_t clockid, struct timespec * tv )
{
    tracer.Trace( "  clock_gettime, clockid %d == %s\n", clockid, get_clockid( clockid ) );

    if ( CLOCK_REALTIME == clockid || CLOCK_REALTIME_COARSE == clockid )
    {
        system_clock::duration d = system_clock::now().time_since_epoch();
        uint64_t diff = duration_cast<nanoseconds>( d ).count();
        tv->tv_sec = diff / 1000000000UL;
        tv->tv_nsec = diff % 1000000000UL;
    }
    else if ( CLOCK_MONOTONIC == clockid || CLOCK_MONOTONIC_COARSE == clockid || CLOCK_MONOTONIC_RAW == clockid ||
              CLOCK_PROCESS_CPUTIME_ID == clockid || CLOCK_THREAD_CPUTIME_ID == clockid )
    {
        high_resolution_clock::time_point tNow = high_resolution_clock::now();
        uint64_t diff = duration_cast<std::chrono::nanoseconds>( tNow - g_tAppStart ).count();
        tv->tv_sec = diff / 1000000000UL;
        tv->tv_nsec = diff % 1000000000UL;
    }

    return 0;
} //clock_gettime

#endif

struct SysCall
{
    const char * name;
    uint64_t id;
};

static const SysCall syscalls[] =
{
    { "SYS_ioctl", SYS_ioctl },
    { "SYS_mkdirat", SYS_mkdirat },
    { "SYS_unlinkat", SYS_unlinkat },
    { "SYS_chdir", SYS_chdir },
    { "SYS_openat", SYS_openat },
    { "SYS_close", SYS_close },
    { "SYS_lseek", SYS_lseek },
    { "SYS_read", SYS_read },
    { "SYS_write", SYS_write },
    { "SYS_writev", SYS_writev },
    { "SYS_pselect6", SYS_pselect6 },
    { "SYS_readlinkat", SYS_readlinkat },
    { "SYS_newfstat", SYS_newfstat },
    { "SYS_fstat", SYS_fstat },
    { "SYS_fdatasync", SYS_fdatasync },
    { "SYS_exit", SYS_exit },
    { "SYS_exit_group", SYS_exit_group },
    { "SYS_set_tid_address", SYS_set_tid_address },
    { "SYS_futex", SYS_futex },
    { "SYS_set_robust_list", SYS_set_robust_list },
    { "SYS_clock_gettime", SYS_clock_gettime },
    { "SYS_tgkill", SYS_tgkill },
    { "SYS_rt_sigprocmask", SYS_rt_sigprocmask },
    { "SYS_gettimeofday", SYS_gettimeofday },
    { "SYS_getpid", SYS_getpid },
    { "SYS_gettid", SYS_gettid },
    { "SYS_sysinfo", SYS_sysinfo },
    { "SYS_brk", SYS_brk },
    { "SYS_mmap", SYS_mmap },
    { "SYS_mprotect", SYS_mprotect },
    { "SYS_riscv_flush_icache", SYS_riscv_flush_icache },
    { "SYS_prlimit64", SYS_prlimit64 },
    { "SYS_getrandom", SYS_getrandom },
    { "SYS_open", SYS_open }, // only called for older systems
    { "rvos_sys_rand", rvos_sys_rand },
    { "rvos_sys_print_double", rvos_sys_print_double },
    { "rvos_sys_trace_instructions", rvos_sys_trace_instructions },
    { "rvos_sys_exit", rvos_sys_exit },
    { "rvos_sys_print_text", rvos_sys_print_text },
    { "rvos_sys_get_datetime", rvos_sys_get_datetime },
};

int syscall_find_compare( const void * a, const void * b )
{
    SysCall & sa = * (SysCall *) a;
    SysCall & sb = * (SysCall *) b;

    if ( sa.id > sb.id )
        return 1;

    if ( sa.id < sb.id )
        return -1;

    return 0;
} //syscall_find_compare

const char * lookup_syscall( uint64_t x )
{
#ifndef NDEBUG

    // ensure they're sorted
    for ( size_t i = 0; i < _countof( syscalls ) - 1; i++ )
        assert( syscalls[ i ].id < syscalls[ i + 1 ].id );

#endif

    SysCall key = { 0, x };
    SysCall * presult = (SysCall *) bsearch( &key, syscalls, _countof( syscalls ), sizeof( key ), syscall_find_compare );

    if ( 0 != presult )
        return presult->name;

    return "unknown";
} //lookup_syscall

bool g_TRACENOW = 0;

void riscv_check_ptracenow( RiscV & cpu )
{
    if ( 0 != g_ptracenow && 0 != *g_ptracenow )
        cpu.trace_instructions( true );
    else
        cpu.trace_instructions( false );
}

// this is called when the risc-v app has an ecall instruction
// https://thevivekpandey.github.io/posts/2017-09-25-linux-system-calls.html

void riscv_invoke_ecall( RiscV & cpu )
{
    if ( tracer.IsEnabled() )
        tracer.Trace( "invoke_ecall %s a7 %llx = %lld, a0 %llx, a1 %llx, a2 %llx, a3 %llx\n", 
                      lookup_syscall( cpu.regs[ RiscV::a7] ), cpu.regs[ RiscV::a7 ], cpu.regs[ RiscV::a7 ],
                      cpu.regs[ RiscV::a0 ], cpu.regs[ RiscV::a1 ], cpu.regs[ RiscV::a2 ], cpu.regs[ RiscV::a3 ] );

    switch ( cpu.regs[ RiscV::a7 ] )
    {
        case rvos_sys_exit: // exit
        case SYS_exit:
        case SYS_exit_group:
        case SYS_tgkill:
        {
            tracer.Trace( "  rvos command 1: exit app\n" );
            g_terminate = true;
            cpu.end_emulation();
            g_exit_code = (int) cpu.regs[ RiscV::a0 ];
            break;
        }
        case rvos_sys_print_text: // print asciiz in a0
        {
            tracer.Trace( "  rvos command 2: print string '%s'\n", (char *) cpu.getmem( cpu.regs[ RiscV::a0 ] ) );
            printf( "%s", (char *) cpu.getmem( cpu.regs[ RiscV::a0 ] ) );
            break;
        }
        case rvos_sys_get_datetime: // writes local date/time into string pointed to by a0
        {
            char * pdatetime = (char *) cpu.getmem( cpu.regs[ RiscV::a0 ] );
            system_clock::time_point now = system_clock::now();
            uint64_t ms = duration_cast<milliseconds>( now.time_since_epoch() ).count() % 1000;
            time_t time_now = system_clock::to_time_t( now );
            struct tm * plocal = localtime( & time_now );
            sprintf( pdatetime, "%02u:%02u:%02u.%03u", plocal->tm_hour, plocal->tm_min, plocal->tm_sec, (uint32_t) ms );
            cpu.regs[ RiscV::a0 ] = 0;
            tracer.Trace( "  got datetime: '%s', pc: %llx\n", pdatetime, cpu.pc );
            break;
        }
        case SYS_fstat:
        {
            tracer.Trace( "  rvos command SYS_fstat\n" );
            int descriptor = (int) cpu.regs[ RiscV::a0 ];

#ifdef _MSC_VER

            // ignore the folder argument on Windows
            struct _stat64 local_stat = {0};
            fill_pstat_windows( descriptor, & local_stat );
            memcpy( cpu.getmem( cpu.regs[ RiscV::a1 ] ), & local_stat, get_min( sizeof( struct _stat64 ), (size_t) 128 ) );
            cpu.regs[ RiscV::a0 ] = 0;

#else

            tracer.Trace( "size of struct stat: %zd\n", sizeof( struct stat ) );
            struct stat local_stat = {0};
            int ret = fstat( descriptor, & local_stat );
            tracer.Trace( "  return code from fstatat: %d, errno %d\n", ret, errno );
            if ( -1 == ret )
            { 
                if ( 0 != g_perrno )
                    *g_perrno = ret;
            }
            else
                memcpy( cpu.getmem( cpu.regs[ RiscV::a1 ] ), & local_stat, get_min( sizeof( struct stat ), (size_t) 128 ) );
 
            cpu.regs[ RiscV::a0 ] = ret;

#endif

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
        case SYS_lseek:
        {
            tracer.Trace( "  rvos command SYS_lseek\n" );

            int descriptor = (int) cpu.regs[ RiscV::a0 ];
            int offset = (int) cpu.regs[ RiscV::a1 ];
            int origin = (int) cpu.regs[ RiscV::a2 ];

            long result = lseek( descriptor, offset, origin );
            tracer.Trace( "  lseek result: %ld\n", result );

            if ( ( -1 == result ) && g_perrno )
                *g_perrno = errno;

            cpu.regs[ RiscV::a0 ] = result;
            break;
        }
        case SYS_read:
        {
            int descriptor = (int) cpu.regs[ RiscV::a0 ];
            void * buffer = cpu.getmem( cpu.regs[ RiscV::a1 ] );
            unsigned buffer_size = (unsigned) cpu.regs[ RiscV::a2 ];
            tracer.Trace( "  rvos command SYS_read. descriptor %d, buffer %llx, buffer_size %u\n", descriptor, cpu.regs[ RiscV::a1 ], buffer_size );

            int result = read( descriptor, buffer, buffer_size );
            tracer.Trace( "  read result: %ld\n", result );

            if ( ( -1 == result ) && g_perrno )
                *g_perrno = errno;

            cpu.regs[ RiscV::a0 ] = result;
            break;
        }
        case SYS_write:
        {
            tracer.Trace( "  rvos command SYS_write. fd %lld, buf %llx, count %lld\n", cpu.regs[ RiscV::a0 ], cpu.regs[ RiscV::a1 ], cpu.regs[ RiscV::a2 ] );

            int descriptor = (int) cpu.regs[ RiscV::a0 ];
            uint8_t * p = (uint8_t *) cpu.getmem( cpu.regs[ RiscV::a1 ] );
            uint64_t count = cpu.regs[ RiscV::a2 ];

            if ( 1 == descriptor || 2 == descriptor ) // stdout / stderr
            {
                tracer.Trace( "  writing '%.*s'\n", (int) count, p );
                printf( "%.*s", (int) count, p );
                cpu.regs[ RiscV::a0 ] = count;
            }
            else if ( 0 == descriptor ) // stdin
            {
                cpu.regs[ RiscV::a0 ] = -1;
                if ( g_perrno )
                    *g_perrno = EACCES;
            }
            else
            {
                size_t result = write( descriptor, p, (int) count );
                if ( ( -1 == result ) && g_perrno )
                    *g_perrno = errno;
                cpu.regs[ RiscV::a0 ] = result;
            }
            break;
        }
        case SYS_open:
        {
            tracer.Trace( "  rvos command SYS_open\n" );

            // a0: asciiz string of file to open. a1: flags. a2: mode

            const char * pname = (const char *) cpu.getmem( cpu.regs[ RiscV::a0 ] );
            int flags = (int) cpu.regs[ RiscV::a1 ];
            int mode = (int) cpu.regs[ RiscV::a2 ];

            tracer.Trace( "  open flags %x, mode %x, file %s\n", flags, mode, pname );

#ifdef _MSC_VER
            flags = windows_translate_flags( flags );
#endif

            int descriptor = open( pname, flags, mode );
            tracer.Trace( "  descriptor: %d, errno %d\n", descriptor, ( -1 == descriptor ) ? errno : 0 );

            if ( -1 == descriptor )
            {
                if ( g_perrno )
                    *g_perrno = errno;
                cpu.regs[ RiscV::a0 ] = ~0;
            }
            else
            {
                if ( g_perrno )
                    *g_perrno = 0;
                cpu.regs[ RiscV::a0 ] = descriptor;
            }
            break;
        }
        case SYS_close:
        {
            tracer.Trace( "  rvos command SYS_close\n" );
            int descriptor = (int) cpu.regs[ RiscV::a0 ];

            if ( descriptor >=0 && descriptor <= 3 )
            {
                // built-in handle stdin, stdout, stderr -- ignore
            }
            else
            {
                int result = close( descriptor );

                if ( -1 == result && g_perrno )
                    *g_perrno = errno;

                cpu.regs[ RiscV::a0 ] = result;
            }
            break;
        }
        case SYS_brk:
        {
            uint64_t original = g_brk_address;
            uint64_t ask = cpu.regs[ RiscV::a0 ];
            if ( 0 == ask )
                cpu.regs[ RiscV::a0 ] = cpu.get_vm_address( g_brk_address );
            else
            {
                uint64_t ask_offset = ask - g_base_address;

                tracer.Trace( "  ask_offset %llx, g_end_of_data %llx, end_of_stack %llx\n", ask_offset, g_end_of_data, g_bottom_of_stack );

                if ( ask_offset >= g_end_of_data && ask_offset < g_bottom_of_stack )
                {
                    g_brk_address = cpu.getoffset( ask );
                    if ( g_brk_address > g_highwater_brk )
                        g_highwater_brk = g_brk_address;
                }
                else
                {
                    tracer.Trace( "  allocation request was too large, failing it by returning current brk\n" );
                    cpu.regs[ RiscV::a0 ] = cpu.get_vm_address( g_brk_address );
                }
            }

            tracer.Trace( "  SYS_brk. ask %llx, current brk %llx, new brk %llx, result in a0 %llx\n", ask, original, g_brk_address, cpu.regs[ RiscV::a0 ] );
            break;
        }
        case rvos_sys_rand: // rand64. returns an unsigned random number in a0
        {
            tracer.Trace( "  rvos command generate random number\n" );
            cpu.regs[ RiscV::a0 ] = rand64();
            break;
        }
        case rvos_sys_print_double: // print_double in a0
        {
            tracer.Trace( "  rvos command print double in a0\n" );
            double d;
            memcpy( &d, &cpu.regs[ RiscV::a0 ], sizeof d );
            printf( "%lf", d );
            break;
        }
        case rvos_sys_trace_instructions:
        {
            tracer.Trace( "  rvos command trace_instructions %d\n", cpu.regs[ RiscV::a0 ] );
            cpu.regs[ RiscV::a0 ] = cpu.trace_instructions( 0 != cpu.regs[ RiscV::a0 ] );
            break;
        }
        case SYS_mmap:
        {
            // The gnu c runtime is ok with this failing -- it just allocates memory instead

            tracer.Trace( "  SYS_mmap\n" );
            cpu.regs[ RiscV::a0 ] = -1;
            break;
        }
#if !defined(OLDGCC) // the syscalls below aren't invoked from the old g++ compiler and runtime
        case SYS_openat:
        {
            tracer.Trace( "  rvos command SYS_openat\n" );
            // a0: asciiz string of file to open. a1: flags. a2: mode

            int directory = (int) cpu.regs[ RiscV::a0 ];
#ifdef __APPLE__
            if ( -100 == directory )
                directory = -2; // Linux vs. MacOS
#endif
            const char * pname = (const char *) cpu.getmem( cpu.regs[ RiscV::a1 ] );
            int flags = (int) cpu.regs[ RiscV::a2 ];
            int mode = (int) cpu.regs[ RiscV::a3 ];

            tracer.Trace( "  open dir %d, flags %x, mode %x, file %s\n", directory, flags, mode, pname );

#ifdef _MSC_VER
            flags = windows_translate_flags( flags );

            // bugbug: directory ignored and assumed to be local
            int descriptor = open( pname, flags, mode );
#else
            int descriptor = openat( directory, pname, flags, mode );
#endif

            tracer.Trace( "  descriptor: %d, errno %d\n", descriptor, ( -1 == descriptor ) ? errno : 0 );

            if ( -1 == descriptor )
            {
                if ( g_perrno )
                    *g_perrno = errno;
                cpu.regs[ RiscV::a0 ] = ~0;
            }
            else
            {
                if ( g_perrno )
                    *g_perrno = 0;
                cpu.regs[ RiscV::a0 ] = descriptor;
            }
            break;
        }
        case SYS_sysinfo:
        {
#if defined(_MSC_VER) || defined(__APPLE__)
            cpu.regs[ RiscV::a0 ] = -1;
#else
            int ret = sysinfo( (struct sysinfo *) cpu.getmem( cpu.regs[ RiscV::a0 ] ) );
            if ( -1 == ret && 0 != g_perrno )
                *g_perrno = ret;
            cpu.regs[ RiscV::a0 ] = ret;
#endif
            break;
        }
        case SYS_newfstat:
        {
            const char * path = (char *) cpu.getmem( cpu.regs[ RiscV::a1 ] );
            tracer.Trace( "  rvos command SYS_newfstat, id %ld, path '%s', flags %llx\n", cpu.regs[ RiscV::a0 ], path, cpu.regs[ RiscV::a3 ] );
            int descriptor = (int) cpu.regs[ RiscV::a0 ];

#ifdef _MSC_VER
            // ignore the folder argument on Windows
            struct _stat64 local_stat = {0};
            fill_pstat_windows( descriptor, & local_stat );
            memcpy( cpu.getmem( cpu.regs[ RiscV::a1 ] ), & local_stat, get_min( sizeof( struct _stat64 ), (size_t) 128 ) );
            cpu.regs[ RiscV::a0 ] = 0;
#else
            tracer.Trace( "size of struct stat: %zd\n", sizeof( struct stat ) );
            struct stat local_stat = {0};
            int ret = fstatat( descriptor, path, & local_stat, cpu.regs[ RiscV::a3 ]  );
            tracer.Trace( "  return code from fstatat: %d, errno %d\n", ret, errno );
            if ( -1 == ret )
            { 
                if ( 0 != g_perrno )
                    *g_perrno = ret;
            }
            else
                memcpy( cpu.getmem( cpu.regs[ RiscV::a1 ] ), & local_stat, get_min( sizeof( struct stat ), (size_t) 128 ) );
 
            cpu.regs[ RiscV::a0 ] = ret;
#endif

            break;
        }
        case SYS_chdir:
        {
            const char * path = (const char *) cpu.getmem( cpu.regs[ RiscV::a0 ] );
            tracer.Trace( "  rvos command SYS_chdir path %s", path );
#ifdef _MSC_VER
            int result = _chdir( path );
#else
            int result = chdir( path );
#endif
            if ( -1 == result && 0 != g_perrno )
                *g_perrno = errno;
            cpu.regs[ RiscV::a0 ] = result;            
            break;
        }
        case SYS_mkdirat:
        {
            int directory = (int) cpu.regs[ RiscV::a0 ];
            const char * path = (const char *) cpu.getmem( cpu.regs[ RiscV::a1 ] );
#ifdef _MSC_VER // on windows ignore the directory argument
            tracer.Trace( "  rvos command SYS_mkdirat path %s\n", path );
            int result = _mkdir( path );
#else
            mode_t mode = (mode_t) cpu.regs[ RiscV::a2 ];
            tracer.Trace( "  rvos command SYS_mkdirat dir %d, path %s, mode %x\n", directory, path, mode );
            int result = mkdirat( directory, path, mode );
#endif

            if ( -1 == result && 0 != g_perrno )
                *g_perrno = errno;
            cpu.regs[ RiscV::a0 ] = result;            
            break;
        }
        case SYS_unlinkat:
        {
            int directory = (int) cpu.regs[ RiscV::a0 ];
            const char * path = (const char *) cpu.getmem( cpu.regs[ RiscV::a1 ] );
            int flags = (int) cpu.regs[ RiscV::a2 ];
            tracer.Trace( "  rvos command SYS_unlinkat dir %d, path %s, flags %x", directory, path, flags );
#ifdef _MSC_VER // on windows ignore the directory argument
            int result = _unlink( path );
#else
            int result = unlinkat( directory, path, flags );
#endif

            if ( -1 == result && 0 != g_perrno )
                *g_perrno = errno;
            cpu.regs[ RiscV::a0 ] = result;
            break;
        }
        case SYS_futex: 
        {
            uint32_t * paddr = (uint32_t *) cpu.getmem( cpu.regs[ RiscV::a0 ] );
            int futex_op = (int) cpu.regs[ RiscV::a1 ] & (~128); // strip "private" flags
            uint32_t value = (uint32_t) cpu.regs[ RiscV::a2 ];

            //tracer.Trace( "  futex all paddr %p (%d), futex_op %d, val %d\n", paddr, ( 0 == paddr ) ? -666 : *paddr, futex_op, value );

            if ( 0 == futex_op ) // FUTEX_WAIT
            {
                if ( *paddr != value )
                    cpu.regs[ RiscV::a0 ] = 11; // EAGAIN
                else
                    cpu.regs[ RiscV::a0 ] = 0;
            }    
            else if ( 1 == futex_op ) // FUTEX_WAKE
                cpu.regs[ RiscV::a0 ] = 0;
            else    
                cpu.regs[ RiscV::a0 ] = -1; // fail this until/unless there is a real-world use
            break;
        }
        case SYS_writev:
        {
            int descriptor = (int) cpu.regs[ RiscV::a0 ];
            const struct iovec * pvec = (const struct iovec *) cpu.getmem( cpu.regs[ RiscV::a1 ] );
            if ( 1 == descriptor || 2 == descriptor )
                tracer.Trace( "  desc %d: writing '%.*s'\n", descriptor, pvec->iov_len, cpu.getmem( (uint64_t) pvec->iov_base ) );

#ifdef _MSC_VER
            size_t result = write( descriptor, cpu.getmem( (uint64_t) pvec->iov_base ), (unsigned) pvec->iov_len );
#else
            struct iovec vec_local;
            vec_local.iov_base = cpu.getmem( (uint64_t) pvec->iov_base );
            vec_local.iov_len = pvec->iov_len;
            size_t result = writev( descriptor, &vec_local, cpu.regs[ RiscV::a2 ] );
#endif

            if ( -1 == result && 0 != g_perrno )
                *g_perrno = errno;
            cpu.regs[ RiscV::a0 ] = result;
            break;
        }
        case SYS_clock_gettime:
        {
            clockid_t cid = (clockid_t) cpu.regs[ RiscV::a0 ];

            #ifdef __APPLE__ // Linux vs MacOS
                if ( 1 == cid )
                    cid = CLOCK_REALTIME;
                else if ( 5 == cid )
                    cid = CLOCK_REALTIME;
            #endif

            int result = clock_gettime( cid, (struct timespec *) cpu.getmem( cpu.regs[ RiscV::a1 ] ) );
            if ( -1 == result && 0 != g_perrno )
                *g_perrno = errno;
            cpu.regs[ RiscV::a0 ] = result;
            break;
        }
        case SYS_fdatasync:
        {
            cpu.regs[ RiscV::a0 ] = -1;
            break;
        }
        case SYS_rt_sigprocmask:
        {
            cpu.regs[ RiscV::a0 ] = -1;
            break;
        }
        case SYS_getpid:
        {
            cpu.regs[ RiscV::a0 ] = getpid();
            break;
        }
        case SYS_gettid:
        {
            cpu.regs[ RiscV::a0 ] = 1;
            break;
        }
        case SYS_getrandom:
        {
            void * buf = cpu.getmem( cpu.regs[ RiscV::a0 ] );
            size_t buflen = cpu.regs[ RiscV::a1 ];
            unsigned int flags = (unsigned int) cpu.regs[ RiscV::a2 ];
            ssize_t result = 0;

#if defined(_MSC_VER) || defined(__APPLE__)
            int * pbuf = (int *) buf;
            size_t count = buflen / sizeof( int );
            for ( size_t i = 0; i < count; i++ )
                pbuf[ i ] = (int) rand64();
            result = buflen;
#else
            result = getrandom( buf, buflen, flags );
#endif

            if ( -1 == result && 0 != g_perrno )
                *g_perrno = errno;
            cpu.regs[ RiscV::a0 ] = result;
            break;
        }
        case SYS_riscv_flush_icache :
        {
            cpu.regs[ RiscV::a0 ] = 0;
            break;
        }
        case SYS_set_tid_address:
        case SYS_set_robust_list:
        case SYS_prlimit64:
        case SYS_readlinkat:
        case SYS_mprotect:
        case SYS_ioctl:
        case SYS_pselect6:
        {
            // ignore for now as apps seem to run ok anyway
            break;
        }
#endif // !defined(OLDGCC)
        default:
        {
            printf( "error; ecall invoked with unknown command %lld = %llx, a0 %#llx, a1 %#llx, a2 %#llx\n",
                    cpu.regs[ RiscV::a7 ], cpu.regs[ RiscV::a7 ], cpu.regs[ RiscV::a0 ], cpu.regs[ RiscV::a1 ], cpu.regs[ RiscV::a2 ] );
            //cpu.regs[ RiscV::a0 ] = -1;
        }
    }
} //riscv_invoke_ecall

static const char * register_names[ 32 ] =
{
    "zero", "ra", "sp",  "gp",  "tp", "t0", "t1", "t2",
    "s0",   "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
    "a6",   "a7", "s2",  "s3",  "s4", "s5", "s6", "s7",
    "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};

const char * target_platform()
{
    #ifdef __riscv
        return "riscv";
    #endif

    #ifdef __amd64
        return "amd64";
    #endif

    #ifdef _M_AMD64    
        return "amd64";
    #endif

    #ifdef _M_ARM64    
        return "arm64";
    #endif

    return "";
} //target_platform

void riscv_hard_termination( RiscV & cpu, const char *pcerr, uint64_t error_value )
{
    tracer.Trace( "rvos (%s) fatal error: %s %llx\n", target_platform(), pcerr, error_value );
    printf( "rvos (%s) fatal error: %s %llx\n", target_platform(), pcerr, error_value );

    tracer.Trace( "pc: %llx %s\n", cpu.pc, riscv_symbol_lookup( cpu, cpu.pc ) );
    printf( "pc: %llx %s\n", cpu.pc, riscv_symbol_lookup( cpu, cpu.pc ) );

    tracer.Trace( "address space %llx to %llx\n", g_base_address, g_base_address + memory.size() );
    printf( "address space %llx to %llx\n", g_base_address, g_base_address + memory.size() );

    tracer.Trace( "  " );
    printf( "  " );
    for ( size_t i = 0; i < 32; i++ )
    {
        tracer.Trace( "%4s: %16llx, ", register_names[ i ], cpu.regs[ i ] );
        printf( "%4s: %16llx, ", register_names[ i ], cpu.regs[ i ] );

        if ( 3 == ( i & 3 ) )
        {
            tracer.Trace( "\n  " );
            printf( "\n  " );
        }
    }
    exit( -1 );
} //riscv_hard_termination

int symbol_find_compare( const void * a, const void * b )
{
    ElfSymbol64 & sa = * (ElfSymbol64 *) a;
    ElfSymbol64 & sb = * (ElfSymbol64 *) b;

    if ( 0 == sa.size ) // a is the key
    {
        if ( sa.value >= sb.value && sa.value < ( sb.value + sb.size ) )
            return 0;
    }
    else // b is the key
    {
        if ( sb.value >= sa.value && sb.value < ( sa.value + sa.size ) )
            return 0;
    }

    if ( sa.value > sb.value )
        return 1;
    return -1;
} //symbol_find_compare

vector<char> g_string_table;      // strings in the elf image
vector<ElfSymbol64> g_symbols;    // symbols in the elf image

// returns the best guess for a symbol name for the address

const char * riscv_symbol_lookup( RiscV & cpu, uint64_t address )
{
    if ( address < g_base_address || address > ( g_base_address + memory.size() ) )
        return "";

    ElfSymbol64 key = {0};
    key.value = address;

    ElfSymbol64 * psym = (ElfSymbol64 *) bsearch( &key, g_symbols.data(), g_symbols.size(), sizeof( key ), symbol_find_compare );

    if ( 0 != psym )
        return & g_string_table[ psym->name ];

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

bool load_image( const char * pimage, const char * app_args )
{
    tracer.Trace( "loading image %s\n", pimage );

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

    g_execution_address = ehead.entry_point;
    g_compressed_rvc = 0 != ( ehead.flags & 1 ); // 2-byte compressed RVC instructions, not 4-byte default risc-v instructions

    // determine how much RAM to allocate

    uint64_t memory_size = 0;

    for ( uint16_t ph = 0; ph < ehead.program_header_table_entries; ph++ )
    {
        size_t o = ehead.program_header_table + ( ph * ehead.program_header_table_size );
        tracer.Trace( "program header %u at offset %zu\n", ph, o );

        ElfProgramHeader64 head = {0};

        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, get_min( sizeof( head ), (size_t) ehead.program_header_table_size ), 1, fp );
        if ( 1 != read )
            usage( "can't read program header" );

        tracer.Trace( "  type: %x / %s\n", head.type, head.show_type() );
        tracer.Trace( "  offset in image: %llx\n", head.offset_in_image );
        tracer.Trace( "  virtual address: %llx\n", head.virtual_address );
        tracer.Trace( "  physical address: %llx\n", head.physical_address );
        tracer.Trace( "  file size: %llx\n", head.file_size );
        tracer.Trace( "  memory size: %llx\n", head.mem_size );
        tracer.Trace( "  alignment: %llx\n", head.alignment );

        if ( 2 == head.type )
        {
            printf( "dynamic linking is not supported by rvos. link with -static\n" );
            exit( 1 );
        }

        memory_size += head.mem_size;

        if ( ( 0 != head.physical_address ) && ( ( 0 == g_base_address ) || g_base_address > head.physical_address ) )
            g_base_address = head.physical_address;
    }

    // first load the string table

    for ( uint16_t sh = 0; sh < ehead.section_header_table_entries; sh++ )
    {
        size_t o = ehead.section_header_table + ( sh * ehead.section_header_table_size );
        ElfSectionHeader64 head = {0};

        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, 1, get_min( sizeof( head ), (size_t) ehead.section_header_table_size ), fp );
        if ( 0 == read )
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
        read = fread( &head, 1, get_min( sizeof( head ), (size_t) ehead.section_header_table_size ), fp );
        if ( 0 == read )
            usage( "can't read section header" );

        tracer.Trace( "  type: %x / %s\n", head.type, head.show_type() );
        tracer.Trace( "  flags: %llx / %s\n", head.flags, head.show_flags() );
        tracer.Trace( "  address: %llx\n", head.address );
        tracer.Trace( "  offset: %llx\n", head.offset );
        tracer.Trace( "  size: %llx\n", head.size );

        if ( 2 == head.type )
        {
            g_symbols.resize( head.size / sizeof( ElfSymbol64 ) );
            fseek( fp, (long) head.offset, SEEK_SET );
            read = fread( g_symbols.data(), 1, head.size, fp );
            if ( 0 == read )
                usage( "can't read symbol table\n" );
        }
    }

    // void out the entries that don't have symbol names

    for ( size_t se = 0; se < g_symbols.size(); se++ )
        if ( 0 == g_symbols[se].name )
            g_symbols[se].value = 0;

    qsort( g_symbols.data(), g_symbols.size(), sizeof( ElfSymbol64 ), symbol_compare );

    // remove symbols that don't look like addresses

    while ( g_symbols.size() && ( g_symbols[ 0 ].value < g_base_address ) )
        g_symbols.erase( g_symbols.begin() );

    for ( size_t se = 0; se < g_symbols.size(); se++ )
    {
        if ( 0 == g_symbols[se].size )
        {
            if ( se < ( g_symbols.size() - 1 ) )
                g_symbols[se].size = g_symbols[ se + 1 ].value - g_symbols[ se ].value;
            else
                g_symbols[se].size = g_base_address + memory_size - g_symbols[ se ].value;
        }
    }

    tracer.Trace( "elf image has %zd usable symbols:\n", g_symbols.size() );
    tracer.Trace( "             address              size  name\n" );

    for ( size_t se = 0; se < g_symbols.size(); se++ )
        tracer.Trace( "    %16llx  %16llx  %s\n", g_symbols[ se ].value, g_symbols[ se ].size, & g_string_table[ g_symbols[ se ].name ] );

    if ( 0 == g_base_address )
        usage( "base address of elf image is invalid; physical address required" );

    // allocate space after uninitialized memory for brk to request space and the stack at the end
    // memory map from high to low addresses:
    //     <end of allocated memory>
    //     Linux start data on the stack (see details below)
    //     g_top_of_stack
    //     g_bottom_of_stack
    //     (unallocated space between brk and the bottom of the stack)
    //     g_brk_address with uninitialized RAM (just after arg_data initially)
    //     g_end_of_data
    //     arg_data
    //     uninitalized data (size read from the .elf file)
    //     initialized data (size & data read from the .elf file)
    //     code (read from the .elf file)
    //     g_base_address (offset read from the .elf file)

    // stacks by convention on risc-v are 16-byte aligned. make sure to start aligned

    if ( memory_size & 0xf )
    {
        memory_size += 16;
        memory_size &= ~0xf;
    }

    uint64_t arg_data = memory_size;
    memory_size += g_args_commit;
    g_end_of_data = memory_size;
    g_brk_address = memory_size;
    g_highwater_brk = memory_size;
    memory_size += g_brk_commit;

    g_bottom_of_stack = memory_size;
    memory_size += g_stack_commit;

    memory.resize( memory_size );
    memset( memory.data(), 0, memory_size );

    // find errno if it exists (only works with older C runtimes that use a global variable)

    for ( size_t se = 0; se < g_symbols.size(); se++ )
    {
        if ( !strcmp( "g_TRACENOW", & g_string_table[ g_symbols[ se ].name ] ) )
            g_ptracenow = (bool *) ( memory.data() + g_symbols[ se ].value - g_base_address );
        else if ( !strcmp( "errno", & g_string_table[ g_symbols[ se ].name ] ) )
            g_perrno = (int *) ( memory.data() + g_symbols[ se ].value - g_base_address );
    }

    // load the program into RAM

    for ( uint16_t ph = 0; ph < ehead.program_header_table_entries; ph++ )
    {
        size_t o = ehead.program_header_table + ( ph * ehead.program_header_table_size );
        ElfProgramHeader64 head = {0};
        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, 1, get_min( sizeof( head ), (size_t) ehead.program_header_table_size ), fp );

        if ( 0 != head.file_size && 0 != head.physical_address )
        {
            fseek( fp, (long) head.offset_in_image, SEEK_SET );
            read = fread( memory.data() + head.physical_address - g_base_address, 1, head.file_size, fp );
            if ( 0 == read )
                usage( "can't read image" );
        }
    }

    // write the command-line arguments into the vm memory in a place where _start can find them.
    // there's an array of pointers to the args followed by the arg strings at offset arg_data.

    uint64_t * parg_data = (uint64_t *) ( memory.data() + arg_data );
    const uint32_t max_args = 40;
    char * buffer_args = ( char *) & ( parg_data[ max_args ] );
    size_t image_len = strlen( pimage );
    vector<char> full_command( 2 + image_len + strlen( app_args ) );
    strcpy( full_command.data(), pimage );
    full_command[ image_len ] = ' ';
    strcpy( full_command.data() + image_len + 1, app_args );

    strcpy( buffer_args, full_command.data() );
    char * pargs = buffer_args;

    uint64_t app_argc = 0;
    while ( *pargs && app_argc < max_args )
    {
        while ( ' ' == *pargs )
            pargs++;
    
        char * space = strchr( pargs, ' ' );
        if ( space )
            *space = 0;

        uint64_t offset = pargs - buffer_args;
        parg_data[ app_argc ] = (uint64_t) ( offset + arg_data + g_base_address + max_args * sizeof( uint64_t ) );
        tracer.Trace( "  argument %d is '%s', at vm address %llx\n", app_argc, pargs, parg_data[ app_argc ] );

        app_argc++;
        pargs += strlen( pargs );
    
        if ( space )
            pargs++;
    }

    // put the Linux startup info at the top of the stack. this consists of (from high to low):
    //   two 8-byte random numbers used for stack and pointer guards
    //   optional filler to make sure alignment of all startup info is 16 bytes
    //   AT_NULL aux record
    //   AT_RANDOM aux record -- point to the 2 random 8-byte numbers above
    //   0 environment termination
    //   0..n environment string pointers
    //   0 argv termination
    //   1..n argv string pointers
    //   argc  <<<==== sp should point here when the entrypoint (likely _start) is invoked

    uint64_t * pstack = (uint64_t *) ( memory.data() + memory_size );

    pstack--;
    *pstack = rand64();
    pstack--;
    *pstack = rand64();
    uint64_t prandom = g_base_address + memory_size - 16;

    // ensure that after all of this the stack is 16-byte aligned

    if ( 0 == ( 1 & app_argc ) )
        pstack--;

    pstack -= 2; // the AT_NULL record will be here since memory is initialized to 0

    pstack -= 2;
    AuxProcessStart * paux = (AuxProcessStart *) pstack;    
    paux->a_type = 25; // AT_RANDOM
    paux->a_un.a_val = prandom;

    pstack--; // no environment is supported yet
    pstack--; // the last argv is 0 to indicate the end

    for ( int iarg = (int) app_argc - 1; iarg >= 0; iarg-- )
    {
        pstack--;
        *pstack = parg_data[ iarg ];
    }

    pstack--;
    *pstack = app_argc;

    uint64_t diff = (uint64_t) ( ( memory.data() + memory_size ) - (uint8_t *) pstack );
    g_top_of_stack = g_base_address + memory_size - diff;

    tracer.Trace( "memory map from highest to lowest addresses:\n" );
    tracer.Trace( "  first byte beyond allocated memory:                 %llx\n", g_base_address + memory_size );
    tracer.Trace( "  <random, alignment, aux recs, env, argv>            (%lld bytes)\n", diff );
    tracer.Trace( "  initial stack pointer g_top_of_stack:               %llx\n", g_top_of_stack );
    tracer.Trace( "  <stack>                                             (%lld bytes)\n", g_stack_commit );
    tracer.Trace( "  last byte stack can use (g_bottom_of_stack):        %llx\n", g_bottom_of_stack );
    tracer.Trace( "  <unallocated space between brk and the stack>       (%lld bytes)\n", g_brk_commit );
    tracer.Trace( "  end_of_data / current brk:                          %llx\n", g_base_address + g_end_of_data );
    tracer.Trace( "  <argv data, pointed to by argv array above>\n" );
    tracer.Trace( "  start of argv data:                                 %llx\n", g_base_address + arg_data );
    tracer.Trace( "  <uninitialized data per the .elf file>\n" );
    tracer.Trace( "  <initialized data from the .elf file>\n" );
    tracer.Trace( "  <code from the .elf file>\n" );
    tracer.Trace( "  initial pc execution_addess:                        %llx\n", g_execution_address );
    tracer.Trace( "  <code per the .elf file>\n" );
    tracer.Trace( "  start of the address space per the .elf file:       %llx\n", g_base_address );

    tracer.Trace( "vm memory first byte beyond:     %p\n", memory.data() + memory_size );
    tracer.Trace( "vm memory start:                 %p\n", memory.data() );
    tracer.Trace( "memory_size:                     %#llx == %lld\n", memory_size, memory_size );
    tracer.Trace( "g_perrno:                        %p\n", g_perrno );
    tracer.Trace( "risc-v compressed instructions:  %s\n", g_compressed_rvc ? "yes" : "no" );

    return true;
} //load_image

void elf_info( const char * pimage )
{
    ElfHeader64 ehead = {0};

    FILE * fp = fopen( pimage, "rb" );
    if ( !fp )
        usage( "can't open image file" );

    CFile file( fp );
    size_t read = fread( &ehead, 1, sizeof ehead, fp );

    if ( 0 == read )
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

    if ( 2 != ehead.bit_width )
    {
        printf( "image isn't 64-bit (2), it's %d\n", ehead.bit_width );
        return;
    }

    printf( "header fields:\n" );
    printf( "  bit_width: %d\n", ehead.bit_width );
    printf( "  entry address: %llx\n", ehead.entry_point );
    printf( "  program entries: %u\n", ehead.program_header_table_entries );
    printf( "  program header entry size: %u\n", ehead.program_header_table_size );
    printf( "  program offset: %llu == %llx\n", ehead.program_header_table, ehead.program_header_table );
    printf( "  section entries: %u\n", ehead.section_header_table_entries );
    printf( "  section header entry size: %u\n", ehead.section_header_table_size );
    printf( "  section offset: %llu == %llx\n", ehead.section_header_table, ehead.section_header_table );
    printf( "  flags: %x\n", ehead.flags );

    g_execution_address = ehead.entry_point;
    g_compressed_rvc = 0 != ( ehead.flags & 1 ); // 2-byte compressed RVC instructions, not 4-byte default risc-v instructions

    uint64_t memory_size = 0;

    for ( uint16_t ph = 0; ph < ehead.program_header_table_entries; ph++ )
    {
        size_t o = ehead.program_header_table + ( ph * ehead.program_header_table_size );
        printf( "program header %u at offset %zu\n", ph, o );

        ElfProgramHeader64 head = {0};

        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, 1, get_min( sizeof( head ), (size_t) ehead.program_header_table_size ), fp );
        if ( 0 == read )
            usage( "can't read program header" );

        printf( "  type: %x / %s\n", head.type, head.show_type() );
        printf( "  offset in image: %llx\n", head.offset_in_image );
        printf( "  virtual address: %llx\n", head.virtual_address );
        printf( "  physical address: %llx\n", head.physical_address );
        printf( "  file size: %llx\n", head.file_size );
        printf( "  memory size: %llx\n", head.mem_size );
        printf( "  alignment: %llx\n", head.alignment );

        memory_size += head.mem_size;

        if ( ( 0 != head.physical_address ) && ( ( 0 == g_base_address ) || g_base_address > head.physical_address ) )
            g_base_address = head.physical_address;
    }

    // first load the string table

    vector<char> string_table;

    for ( uint16_t sh = 0; sh < ehead.section_header_table_entries; sh++ )
    {
        size_t o = ehead.section_header_table + ( sh * ehead.section_header_table_size );
        ElfSectionHeader64 head = {0};

        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, 1, get_min( sizeof( head ), (size_t) ehead.section_header_table_size ), fp );
        if ( 0 == read )
            usage( "can't read section header" );

        if ( 3 == head.type )
        {
            string_table.resize( head.size );
            fseek( fp, (long) head.offset, SEEK_SET );
            read = fread( string_table.data(), 1, head.size, fp );
            if ( 0 == read )
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
        read = fread( &head, 1, get_min( sizeof( head ), (size_t) ehead.section_header_table_size ), fp );
        if ( 0 == read )
            usage( "can't read section header" );

        printf( "  type: %x / %s\n", head.type, head.show_type() );
        printf( "  flags: %llx / %s\n", head.flags, head.show_flags() );
        printf( "  address: %llx\n", head.address );
        printf( "  offset: %llx\n", head.offset );
        printf( "  size: %llx\n", head.size );

        if ( 2 == head.type )
        {
            vector<ElfSymbol64> symbols;
            symbols.resize( head.size / sizeof( ElfSymbol64 ) );
            fseek( fp, (long) head.offset, SEEK_SET );
            read = fread( symbols.data(), 1, head.size, fp );
            if ( 0 == read )
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
                printf( "     size:  %llu\n", sym_entry.size );
            }
        }
    }

    if ( 0 == g_base_address )
        printf( "base address of elf image is zero; physical address required for the rvos emulator\n" );

    printf( "contains 2-byte compressed RVC instructions: %s\n", g_compressed_rvc ? "yes" : "no" );
    printf( "vm g_base_address %llx\n", g_base_address );
    printf( "memory_size: %llx\n", memory_size );
    printf( "g_stack_commit: %llx\n", g_stack_commit );
    printf( "g_execution_address %llx\n", g_execution_address );
} //elf_info

int ends_with( const char * str, const char * end )
{
    size_t len = strlen( str );
    size_t lenend = strlen( end );

    if ( len < lenend )
        return false;

    return ( 0 == _stricmp( str + len - lenend, end ) );
} //ends_with

static void RenderNumber( int64_t n, char * ac )
{
    if ( n < 0 )
    {
        strcat( ac, "-" );
        RenderNumber( -n, ac );
        return;
    }
   
    if ( n < 1000 )
    {
        sprintf( ac + strlen( ac ), "%lld", n );
        return;
    }

    RenderNumber( n / 1000, ac );
    sprintf( ac + strlen( ac ), ",%03lld", n % 1000 );
    return;
} //RenderNumber

static char * RenderNumberWithCommas( int64_t n, char * ac )
{
    ac[ 0 ] = 0;
    RenderNumber( n, ac );
    return ac;
} //RenderNumberWithCommas

int main( int argc, char * argv[] )
{
    bool trace = false;
    char * pcApp = 0;
    bool showPerformance = false;
    bool traceInstructions = false;
    bool elfInfo = false;
    bool generateRVCTable = false;
    static char acAppArgs[1024] = {0};
    static char acApp[1024] = {0};

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
            else if ( 'h' == ca )
            {
                if ( ':' != parg[2] )
                    usage( "the -h argument requires a value" );

                uint64_t heap = strtoull( parg + 3 , 0, 10 );
                if ( heap > 1024 ) // limit to a gig
                    usage( "invalid heap size specified" );

                g_brk_commit = heap * 1024 * 1024;
            }
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

    bool ok = load_image( acApp, acAppArgs );
    if ( ok )
    {
        unique_ptr<RiscV> cpu( new RiscV( memory, g_base_address, g_execution_address, g_compressed_rvc, g_stack_commit, g_top_of_stack ) );
        cpu->trace_instructions( traceInstructions );
        uint64_t cycles = 0;

        #ifdef _MSC_VER
            g_tAppStart = high_resolution_clock::now();
        #endif

        high_resolution_clock::time_point tStart = high_resolution_clock::now();

        do
        {
            cycles += cpu->run( 1000000 );
        } while( !g_terminate );

        char ac[ 100 ];
        if ( showPerformance )
        {
            high_resolution_clock::time_point tDone = high_resolution_clock::now();
            int64_t totalTime = duration_cast<std::chrono::milliseconds>( tDone - tStart ).count();

            printf( "elapsed milliseconds:  %15s\n", RenderNumberWithCommas( totalTime, ac ) );
            printf( "RISC-V cycles:         %15s\n", RenderNumberWithCommas( cycles, ac ) );
            printf( "effective clock rate:  %15s\n", RenderNumberWithCommas( cycles / totalTime, ac ) );
            printf( "app exit code:         %15d\n", g_exit_code );
        }

        tracer.Trace( "highwater brk heap:  %15s\n", RenderNumberWithCommas( g_highwater_brk - g_end_of_data, ac ) );
    }
    tracer.Shutdown();

    return g_exit_code;
} //main
