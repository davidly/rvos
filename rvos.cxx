/*
    This emulates an extemely simple RISC-V OS.
    Per https://riscv.org/wp-content/uploads/2017/05/riscv-privileged-v1.10.pdf
    It's an AEE (Application Execution Environment) that exposes an ABI (Application Binary Inferface) for an Application.
    It runs in RISC-V M mode, similar to embedded systems.
    It can load and execute 64-bit RISC-V apps built with Gnu tools in .elf files targeting Linux.
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

#ifdef _WIN32
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
    #include <termios.h>
        #include <sys/random.h>
        #include <sys/uio.h>
        #include <dirent.h>
        #ifndef __APPLE__
            #include <sys/sysinfo.h>
        #endif

        // this structure is smaller than the usermode version. I don't know of a cross-platform header with it.
    
        #define local_KERNEL_NCCS 19
        struct local_kernel_termios
        {
            tcflag_t c_iflag;     /* input mode flags */
            tcflag_t c_oflag;     /* output mode flags */
            tcflag_t c_cflag;     /* control mode flags */
            tcflag_t c_lflag;     /* local mode flags */
            cc_t c_line;          /* line discipline */
            cc_t c_cc[local_KERNEL_NCCS]; /* control characters */
        };
    #endif

    struct LINUX_FIND_DATA
    {
        char cFileName[ MAX_PATH ];
    };
#endif

#include <djltrace.hxx>
#include <djl_con.hxx>

using namespace std;
using namespace std::chrono;

#include "riscv.hxx"
#include "rvos.h"

CDJLTrace tracer;
ConsoleConfiguration g_consoleConfig;
bool g_compressed_rvc = false;                 // is the app compressed risc-v?
const uint64_t g_args_commit = 1024;           // storage spot for command-line arguments and environment variables
const uint64_t g_stack_commit = 128 * 1024;    // RAM to allocate for the fixed stack
uint64_t g_brk_commit = 10 * 1024 * 1024;      // RAM to reserve if the app calls brk to allocate space. 10 meg default
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
bool * g_ptracenow = 0;                        // if this variable exists in the vm, it'll control instruction tracing

// fake descriptors.
// /etc/timezone is not implemented, so apps running in RVOS on Windows assume UTC

const uint64_t findFirstDescriptor = 3000;
const uint64_t timebaseFrequencyDescriptor = 3001;

#pragma warning(disable: 4200) // 0-sized array
struct linux_dirent64_syscall {
    uint64_t d_ino;     /* Inode number */
    uint64_t d_off;     /* Offset to next linux_dirent */
    uint16_t d_reclen;  /* Length of this linux_dirent */
    uint8_t  d_type;    /* DT_DIR (4) if a dir, DT_REG (8) if a regular file */
    char     d_name[];
    /* optional and not implemented. must be 0-filled
    char pad
    char d_type
    */
};

struct timespec_syscall {
    uint64_t tv_sec;
    uint64_t tv_nsec;
};

struct stat_linux_syscall {
    /*
        struct stat info run on a 64-bit RISC-V system
      sizeof s: 128
      offset      size field
           0         8 st_dev
           8         8 st_ino
          16         4 st_mode
          20         4 st_nlink
          24         4 st_uid
          28         4 st_gid
          32         8 st_rdev
          48         8 st_size
          56         4 st_blksize
          64         8 st_blocks
          72        16 st_atim
          88        16 st_mtim
         104        16 st_ctim
    */

    uint64_t   st_dev;      /* ID of device containing file */
    uint64_t   st_ino;      /* Inode number */
    uint32_t   st_mode;     /* File type and mode */
    uint32_t   st_nlink;    /* Number of hard links */
    uint32_t   st_uid;      /* User ID of owner */
    uint32_t   st_gid;      /* Group ID of owner */
    uint64_t   st_rdev;     /* Device ID (if special file) */
    uint64_t   st_mystery_spot;
    uint64_t   st_size;     /* Total size, in bytes */
    uint32_t   st_blksize;  /* Block size for filesystem I/O */
    uint64_t   st_blocks;   /* Number of 512 B blocks allocated */

    /* Since POSIX.1-2008, this structure supports nanosecond
       precision for the following timestamp fields.
       For the details before POSIX.1-2008, see VERSIONS. */

    struct timespec  st_atim;  /* Time of last access */
    struct timespec  st_mtim;  /* Time of last modification */
    struct timespec  st_ctim;  /* Time of last status change */

#ifndef st_atime
#ifndef OLDGCC
    #define st_atime  st_atim.tv_sec  /* Backward compatibility */
    #define st_mtine  st_mtim.tv_sec
    #define st_ctime  st_ctim.tv_sec
#endif
#endif

    uint64_t   st_mystery_spot_2;
};

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
    uint64_t memory_size;
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

char printable( uint8_t x )
{
    if ( x < ' ' || x >= 127 )
        return ' ';
    return x;
} //printable

void usage( char const * perror = 0 )
{
    g_consoleConfig.RestoreConsole( false );

    if ( 0 != perror )
        printf( "error: %s\n", perror );

    printf( "usage: rvos <elf_executable>\n" );
    printf( "   arguments:    -e     just show information about the elf executable; don't actually run it\n" );
    printf( "                 -g     (internal) generate rcvtable.txt then exit\n" );
    printf( "                 -h:X   # of meg for the heap (brk space) 0..1024 are valid. default is 10\n" );
    printf( "                 -i     if -t is set, also enables risc-v instruction tracing\n" );
    printf( "                 -p     shows performance information at app exit\n" );
    printf( "                 -t     enable debug tracing to rvos.log\n" );
    printf( "  %s\n", build_string() );
    exit( 1 );
} //usage

struct linux_timeval
{
    uint64_t tv_sec;       // time_t
    uint64_t tv_usec;      // suseconds_t
};

int gettimeofday( linux_timeval * tp )
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

void backslash_to_slash( char * p )
{
    while ( *p )
    {
        if ( '\\' == *p )
            *p = '/';
        p++;
    }
} //backslash_to_slash

#ifdef _WIN32

void slash_to_backslash( char * p )
{
    while ( *p )
    {
        if ( '/' == *p )
            *p = '\\';
        p++;
    }
} //slash_to_backslash

int windows_translate_flags( int flags )
{
    // Microsoft C uses different constants for flags than linux:
    // O_CREAT == 0x100 msft,        0x200 linux
    // O_TRUNC == 0x200 msft,        0x400 linux
    // O_ASYNC == (undefined)  msft, 0x40 on linux
    // 0x40 ==    O_TEMPORARY  msft, O_ASYNC linux // send SIGIO when data is ready
    // 0x80 ==    O_NOINHERIT  msft, O_FSYNC // synchronous writes
    // 0x20 ==    O_SEQUENTIAL msft, _FDEFER / O_EXLOCK / _FEXLOCK on others
    // 0x10 ==    O_RANDOM     msft, _FMARK / O_SHLOCK / _FSHLOCK / O_SYNC on others

    // don't create a temporary file when the caller asked for async

    flags &= ~ 0x40;

    // don't create a non-inherited file when the caller asked for fsync

    flags &= ~ 0x80;

    // translate O_CREAT

    if ( flags & 0x200 )
    {
        flags |= 0x100;
        flags &= ~ 0x200;
    }

    // translate O_TRUNC

    if ( flags & 0x400 )
    {
        flags |= 0x200;
        flags &= ~ 0x400;
    }

    // turn off 0x10 and 0x20 because they aren't critical and don't translate

    flags &= ~ 0x30;

    flags |= O_BINARY; // this is assumed on Linux systems

    tracer.Trace( "  flags translated for Microsoft: %x\n", flags );
    return flags;
} //windows_translate_flags

int fill_pstat_windows( int descriptor, struct stat_linux_syscall * pstat, const char * path )
{
    char ac[ MAX_PATH ];
    if ( 0 != path )
    {
        strcpy( ac, path );
        slash_to_backslash( ac );
    }
    else
        ac[ 0 ] = 0;

    memset( pstat, 0, sizeof( struct stat_linux_syscall ) );

    // default values, most of which are lies

    pstat->st_ino = 3;
    pstat->st_nlink = 1;
    pstat->st_uid = 1000;
    pstat->st_gid = 5;
    pstat->st_rdev = 1024; // this is st_blksize on linux
    pstat->st_size = 0;

    if ( descriptor >= 0 && descriptor <= 2 ) // stdin / stdout / stderr
        pstat->st_mode = S_IFCHR;
    else if ( findFirstDescriptor == descriptor )
    {
        pstat->st_mode = S_IFDIR;
        pstat->st_rdev = 4096; // this is st_blksize on linux
    }
    else if ( timebaseFrequencyDescriptor == descriptor )
    {
        pstat->st_mode = S_IFREG;
        pstat->st_rdev = 4096; // this is st_blksize on linux
    }
    else
    {
        WIN32_FILE_ATTRIBUTE_DATA data = {0};
        BOOL ok = FALSE;
        if ( ac[ 0 ] )
        {
            ok = GetFileAttributesExA( ac, GetFileExInfoStandard, & data );
            tracer.Trace( "  result of GetFileAttributesEx on '%s': %d\n", ac, ok );
        }

        if ( !ok && ( descriptor < 0 ) )
        {
            errno = 2; // not found;
            return -1;
        }

        if ( ok && ( data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
            pstat->st_mode = S_IFDIR;
        else
            pstat->st_mode = S_IFREG;

        pstat->st_rdev = 4096; // this is st_blksize on linux

        if ( !ok && ( descriptor > 0 ) )
            data.nFileSizeLow = portable_filelen( descriptor );

        pstat->st_size = data.nFileSizeLow;
    }

    return 0;
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
    "monotonic_coarse",
};

typedef int clockid_t;

const char * get_clockid( clockid_t clockid )
{
    if ( clockid < _countof( clockids ) )
        return clockids[ clockid ];
    return "unknown";
} //get_clockid

high_resolution_clock::time_point g_tAppStart;

int msc_clock_gettime( clockid_t clockid, struct timespec_syscall * tv )
{
    tracer.Trace( "  msc_clock_gettime, clockid %d == %s\n", clockid, get_clockid( clockid ) );
    uint64_t diff = 0;

    if ( CLOCK_REALTIME == clockid || CLOCK_REALTIME_COARSE == clockid )
    {
        system_clock::duration d = system_clock::now().time_since_epoch();
        diff = duration_cast<nanoseconds>( d ).count();
    }
    else if ( CLOCK_MONOTONIC == clockid || CLOCK_MONOTONIC_COARSE == clockid || CLOCK_MONOTONIC_RAW == clockid ||
              CLOCK_PROCESS_CPUTIME_ID == clockid || CLOCK_THREAD_CPUTIME_ID == clockid )
    {
        high_resolution_clock::time_point tNow = high_resolution_clock::now();
        diff = duration_cast<std::chrono::nanoseconds>( tNow - g_tAppStart ).count();
    }

    tv->tv_sec = diff / 1000000000ULL;
    tv->tv_nsec = diff % 1000000000ULL;

    return 0;
} //msc_clock_gettime

#endif

#ifdef __APPLE__

// For each of these flag fields o/i/l/c this code translates the subset of the values actually used in the apps
// I validated. There are certainly other cases that are still broken (though many aren't implemented in MacOS,
// so translating them wouldn't have any impact). If anyone understands the historical reasons for the differences
// in these flags I'd love to hear about it. Why did coders knowngly pick different constant values?

tcflag_t map_termios_oflag_linux_to_macos( tcflag_t f )
{
    tcflag_t r = 0;
    if ( f & 1 ) // OPOST is the same bit
        r |= 1;

    if ( f & 4 ) // ONLCR
        r |= 2;

    if ( f & 8 ) // OCRNL
        r |= 0x10;

    if ( f & 0x10 ) // ONOCR
        r |= 0x20;

    if ( f & 0x20 ) // ONLRET
        r |= 0x40;

    return r;
} //map_termios_oflag_linux_to_macos

tcflag_t map_termios_oflag_macos_to_linux( tcflag_t f )
{
    tcflag_t r = 0;
    if ( f & 1 ) // OPOST is the same bit for both
        r |= 1;

    if ( f & 2 ) // ONLCR
        r |= 4;

    if ( f & 0x10 ) // OCRNL
        r |= 8;

    if ( f & 0x20 ) // ONOCR
        r |= 0x10;

    if ( f & 0x40 ) // ONLRET
        r |= 0x20;

    return r;
} //map_termios_oflag_linux_to_macos

tcflag_t map_termios_iflag_linux_to_macos( tcflag_t f )
{
    tcflag_t r = f;

    if ( f & 0x400 ) // IXON
    {
        r &= ~ 0x400;
        r |= 0x200;
    }
    else if ( f & 0x1000 ) // IXOFF
    {
        r &= ~ 0x1000;
        r |= 0x400;
    }

    return r;
} //map_termios_oflag_linux_to_macos

tcflag_t map_termios_iflag_macos_to_linux( tcflag_t f )
{
    tcflag_t r = f;

    if ( f & 0x200 ) // IXON
    {
        r &= ~ 0x200;
        r |= 0x400;
    }
    else if ( f & 0x400 ) // IXOFF
    {
        r &= ~ 0x400;
        r |= 0x1000;
    }

    return r;
} //map_termios_oflag_linux_to_macos

tcflag_t map_termios_lflag_linux_to_macos( tcflag_t f )
{
    tcflag_t r = 0;

    if ( f & 1 ) // ICANON
        r |= 0x80;

    if ( f & 2 ) // ECHONL
        r |= 0x100;

    if ( f & 8 ) // ECHOK
        r |= 8;

    if ( f & 0x10 ) // ECHOKE
        r |= 2;

    if ( f & 0x20 ) // ECHOE
        r |= 4;

    if ( f & 0x40 ) // ECHO
        r |= 0x10;

    if ( f & 0x100 ) // EXTPROC
        r |= 0x400000;

    if ( f & 0x200 ) // ECHOPRT
        r |= 0x40;

    if ( f & 0x400 ) // ECONL
        r |= 0x20;

    if ( f & 0x8000 ) // ISIG
        r |= 0x400;

    if ( f & 0x10000 ) // IEXTEN
        r |= 0x800;

    return r;
} //map_termios_lflag_linux_to_macos

tcflag_t map_termios_lflag_macos_to_linux( tcflag_t f )
{
    tcflag_t r = 0;

    if ( f & 0x80 ) // ICANON
        r |= 1;

    if ( f & 0x100 ) // ECHONL
        r |= 2;

    if ( f & 8 ) // ECHOK
        r |= 8;

    if ( f & 2 ) // ECHOKE
        r |= 0x10;

    if ( f & 4 ) // ECHOE
        r |= 0x20;

    if ( f & 0x10 ) // ECHO
        r |= 0x40;

    if ( f & 0x400000 ) // EXTPROC
        r |= 0x100;

    if ( f & 0x40 ) // ECHOPRT
        r |= 0x200;

    if ( f & 0x20 ) // ECONL
        r |= 0x400;

    if ( f & 0x400 ) // ISIG
        r |= 0x8000;

    if ( f & 0x800 ) // IEXTEN
        r |= 0x10000;

    return r;
} //map_termios_lflag_linux_to_macos

tcflag_t map_termios_cflag_linux_to_macos( tcflag_t f )
{
    tcflag_t r = 0;

    if ( f & 0x10 ) // CS5
        r |= 0x100;

    if ( f & 0x20 ) // CS6  CLOCAL is also 0x20 on linux and 0x400 on MacOS
        r |= 0x200;

    if ( f & 0x30 ) // CS7
        r |= 0x300;

    if ( f & 0x40 ) // CSIZE
        r |= 0x400;

    if ( f & 0x80 ) // CSTOPB
        r |= 0x800;

    if ( f & 0x100 ) // CREAD
        r |= 0x1000;

    if ( f & 0x200 ) // PARENB
        r |= 0x2000;

    if ( f & 0x400 ) // CS8
        r |=  0x4000;

    if ( f & 0x800 ) // HUPCL
        r |= 0x8000;

    return r;
} //map_termios_cflag_linux_to_macos

tcflag_t map_termios_cflag_macos_to_linux( tcflag_t f )
{
    tcflag_t r = 0;

    if ( f & 0x100 ) // CS5
        r |= 0x10;

    if ( f & 0x200 ) // CS6  CLOCAL is also 0x20 on linux and 0x400 on MacOS
        r |= 0x20;

    if ( f & 0x300 ) // CS7
        r |= 0x30;

    if ( f & 0x400 ) // CSIZE
        r |= 0x40;

    if ( f & 0x800 ) // CSTOPB
        r |= 0x80;

    if ( f & 0x1000 ) // CREAD
        r |= 0x100;

    if ( f & 0x2000 ) // PARENB
        r |= 0x200;

    if ( f & 0x4000 ) // CS8
        r |=  0x400;

    if ( f & 0x8000 ) // HUPCL
        r |= 0x800;

    return r;
} //map_termios_cflag_linux_to_macos

#endif

#if defined(__ARM_32BIT_STATE) || defined(__ARM_64BIT_STATE)
    int linux_riscv64_to_arm_open_flags( int flags )
    {
        // values are the same aside from these, which are flipped:
        //               riscv64      arm32 and arm64
        // O_DIRECT      0x4000       0x10000
        // O_DIRECTORY   0x10000      0x4000

        int r = flags;
        if ( flags & 0x4000 )
        {
            r &= ~0x4000;
            r |= 0x10000;
        }
        if ( flags & 0x10000 )
        {
            r &= ~0x10000;
            r |= 0x4000;
        }
        return r;
    } //linux_risc64v_to_arm_open_flags
#endif

struct SysCall
{
    const char * name;
    uint64_t id;
};

static const SysCall syscalls[] =
{
    { "SYS_getcwd", SYS_getcwd },
    { "SYS_ioctl", SYS_ioctl },
    { "SYS_mkdirat", SYS_mkdirat },
    { "SYS_unlinkat", SYS_unlinkat },
    { "SYS_chdir", SYS_chdir },
    { "SYS_openat", SYS_openat },
    { "SYS_close", SYS_close },
    { "SYS_getdents64", SYS_getdents64 },
    { "SYS_lseek", SYS_lseek },
    { "SYS_read", SYS_read },
    { "SYS_write", SYS_write },
    { "SYS_writev", SYS_writev },
    { "SYS_pselect6", SYS_pselect6 },
    { "SYS_readlinkat", SYS_readlinkat },
    { "SYS_newfstatat", SYS_newfstatat },
    { "SYS_newfstat", SYS_newfstat },
    { "SYS_fdatasync", SYS_fdatasync },
    { "SYS_exit", SYS_exit },
    { "SYS_exit_group", SYS_exit_group },
    { "SYS_set_tid_address", SYS_set_tid_address },
    { "SYS_futex", SYS_futex },
    { "SYS_set_robust_list", SYS_set_robust_list },
    { "SYS_clock_gettime", SYS_clock_gettime },
    { "SYS_clock_nanosleep", SYS_clock_nanosleep },
    { "SYS_sched_setaffinity", SYS_sched_setaffinity },
    { "SYS_sched_getaffinity", SYS_sched_getaffinity },
    { "SYS_tgkill", SYS_tgkill },
    { "SYS_sigaction", SYS_sigaction },
    { "SYS_rt_sigprocmask", SYS_rt_sigprocmask },
    { "SYS_prctl", SYS_prctl },
    { "SYS_gettimeofday", SYS_gettimeofday },
    { "SYS_getpid", SYS_getpid },
    { "SYS_gettid", SYS_gettid },
    { "SYS_sysinfo", SYS_sysinfo },
    { "SYS_brk", SYS_brk },
    { "SYS_mmap", SYS_mmap },
    { "SYS_mprotect", SYS_mprotect },
    { "SYS_riscv_flush_icache", SYS_riscv_flush_icache },
    { "SYS_prlimit64", SYS_prlimit64 },
    { "SYS_renameat2", SYS_renameat2 },
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
} //riscv_check_ptracenow

void update_a0_errno(  RiscV & cpu, int64_t result )
{
    if ( result >= 0 || result <= -4096 ) // syscalls like write() return positive values to indicate success.
    {
        tracer.Trace( "  syscall success, returning %lld\n", result );
        cpu.regs[ RiscV::a0 ] = result;
    }
    else
    {
        tracer.Trace( "  returning negative errno in a0: %d\n", -errno );
        cpu.regs[ RiscV::a0 ] = -errno; // it looks like the g++ runtime copies the - of this value to errno
    }
} //update_a0_errno

// this is called when the risc-v app has an ecall instruction
// https://thevivekpandey.github.io/posts/2017-09-25-linux-system-calls.html

#pragma warning(disable: 4189) // unreferenced local variable

void riscv_invoke_ecall( RiscV & cpu )
{
#ifdef _WIN32
    static HANDLE g_hFindFirst = INVALID_HANDLE_VALUE; // for enumerating directories. Only one can be active at once.
    static char g_acFindFirstPattern[ MAX_PATH ];
    char acPath[ MAX_PATH ];
#else
    #ifndef OLDGCC      // the several-years-old Gnu C compiler for the RISC-V development boards
        static DIR * g_FindFirst = 0;
        static uint64_t g_FindFirstDescriptor = -1;
    #endif
#endif

    // Linux syscalls support up to 6 arguments

    if ( tracer.IsEnabled() )
        tracer.Trace( "invoke_ecall %s a7 %llx = %lld, a0 %llx, a1 %llx, a2 %llx, a3 %llx, a4 %llx, a5 %llx\n", 
                      lookup_syscall( cpu.regs[ RiscV::a7] ), cpu.regs[ RiscV::a7 ], cpu.regs[ RiscV::a7 ],
                      cpu.regs[ RiscV::a0 ], cpu.regs[ RiscV::a1 ], cpu.regs[ RiscV::a2 ], cpu.regs[ RiscV::a3 ],
                      cpu.regs[ RiscV::a4 ], cpu.regs[ RiscV::a5 ] );

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
            fflush( stdout );
            update_a0_errno( cpu, 0 );
            break;
        }
        case rvos_sys_get_datetime: // writes local date/time into string pointed to by a0
        {
            char * pdatetime = (char *) cpu.getmem( cpu.regs[ RiscV::a0 ] );
            system_clock::time_point now = system_clock::now();
            uint64_t ms = duration_cast<milliseconds>( now.time_since_epoch() ).count() % 1000;
            time_t time_now = system_clock::to_time_t( now );
            struct tm * plocal = localtime( & time_now );
            sprintf( pdatetime, "%02u:%02u:%02u.%03u", (uint32_t) plocal->tm_hour, (uint32_t) plocal->tm_min, (uint32_t) plocal->tm_sec, (uint32_t) ms );
            tracer.Trace( "  got datetime: '%s', pc: %llx\n", pdatetime, cpu.pc );
            update_a0_errno( cpu, 0 );
            break;
        }
        case SYS_getcwd:
        {
            // the syscall version of getcwd never uses malloc to allocate a return value. Just C runtimes do that for POSIX compliance.

            uint64_t original = cpu.regs[ RiscV::a0 ];
            tracer.Trace( "  address in vm space: %llx == %llu\n", original, original );
            char * pin = (char *) cpu.getmem( cpu.regs[ RiscV::a0 ] );
            size_t size = (size_t) cpu.regs[ RiscV::a1 ];
            uint64_t pout = 0;
#ifdef _WIN32
            char * poutwin32 = _getcwd( acPath, sizeof( acPath ) );
            if ( poutwin32 )
            {
                tracer.Trace( "  acPath: '%s', poutwin32: '%s'\n", acPath, poutwin32 );
                backslash_to_slash( poutwin32 );
                pout = original;
                strcpy( pin, poutwin32 + 2 ); // path C:
            }
            else
                tracer.Trace( "  _getcwd failed on win32, error %d\n", errno );

            tracer.Trace( "  getcwd returning '%s'\n", pin );
#else
    #ifndef OLDGCC      // the several-years-old Gnu C compiler for the RISC-V development boards
            pout = cpu.host_to_vm_address( getcwd( pin, size ) );
    #endif
#endif
            if ( pout )
                update_a0_errno( cpu, original );
            else
                update_a0_errno( cpu, errno );
            break;
        }
        case SYS_clock_nanosleep:
        {
            clockid_t clockid = (clockid_t) cpu.regs[ RiscV::a0 ];
            int flags = (int) cpu.regs[ RiscV::a1 ];
            tracer.Trace( "  nanosleep id %d flags %x\n", clockid, flags );

            const struct timespec * request = (const struct timespec *) cpu.getmem( cpu.regs[ RiscV::a2 ] );
            struct timespec * remain = 0;
            if ( 0 != cpu.regs[ RiscV::a3 ] )
                remain = (struct timespec *) cpu.getmem( cpu.regs[ RiscV::a3 ] );

            uint64_t ms = request->tv_sec * 1000 + request->tv_nsec / 1000000;
            tracer.Trace( "  nanosleep sec %lld, nsec %lld == %lld ms\n", request->tv_sec, request->tv_nsec, ms );
            sleep_ms( ms );
            update_a0_errno( cpu, 0 );
            break;
        }
        case SYS_sched_setaffinity:
        {
            tracer.Trace( "  setaffinity, EPERM %d\n", EPERM );
            update_a0_errno( cpu, EPERM );
            break;
        }
        case SYS_sched_getaffinity:
        {
            tracer.Trace( "  getaffinity, EPERM %d\n", EPERM );
            update_a0_errno( cpu, EPERM );
            break;
        }
        case SYS_newfstat:
        {
            // two arguments: descriptor and pointer to the stat buf
            tracer.Trace( "  rvos command SYS_newfstat\n" );
            int descriptor = (int) cpu.regs[ RiscV::a0 ];
            int result = 0;

#ifdef _WIN32
            struct stat_linux_syscall local_stat = {0};
            result = fill_pstat_windows( descriptor, & local_stat, 0 );
            if ( 0 == result )
            {
                size_t cbStat = sizeof( struct stat_linux_syscall );
                tracer.Trace( "  sizeof stat_linux_syscall: %zd\n", cbStat );
                assert( 128 == cbStat );  // 128 is the size of the stat struct this syscall on RISC-V Linux
                memcpy( cpu.getmem( cpu.regs[ RiscV::a1 ] ), & local_stat, cbStat );
                tracer.Trace( "  file size in bytes: %zd, offsetof st_size: %zd\n", local_stat.st_size, offsetof( struct stat_linux_syscall, st_size ) );
            }
            else
            {
                errno = 2;
                tracer.Trace( "  fill_pstat_windows failed\n" );
            }
#else
            tracer.Trace( "  sizeof struct stat: %zd\n", sizeof( struct stat ) );
            struct stat local_stat = {0};
            struct stat_linux_syscall local_stat_syscall = {0};
            result = fstat( descriptor, & local_stat );
            if ( 0 == result )
            {
                // the syscall version of stat has similar fields but a different layout, so copy fields one by one

                struct stat_linux_syscall * pout = (struct stat_linux_syscall *) cpu.getmem( cpu.regs[ RiscV::a1 ] );
                pout->st_dev = local_stat.st_dev;
                pout->st_ino = local_stat.st_ino;
                pout->st_mode = local_stat.st_mode;
                pout->st_nlink = local_stat.st_nlink;
                pout->st_uid = local_stat.st_uid;
                pout->st_gid = local_stat.st_gid;
                pout->st_rdev = local_stat.st_rdev;
                pout->st_size = local_stat.st_size;
                pout->st_blksize = local_stat.st_blksize;
                pout->st_blocks = local_stat.st_blocks;
#ifdef __APPLE__
                pout->st_atim = local_stat.st_atimespec;
                pout->st_mtim = local_stat.st_mtimespec;
                pout->st_ctim = local_stat.st_ctimespec;
#elif defined(OLDGCC)
                // no time on old gcc intended for embedded systems
#else
                pout->st_atim = local_stat.st_atim;
                pout->st_mtim = local_stat.st_mtim;
                pout->st_ctim = local_stat.st_ctim;
#endif
                tracer.Trace( "  file size %zd, isdir %s\n", local_stat.st_size, S_ISDIR( local_stat.st_mode ) ? "yes" : "no" );
            }
            else
                tracer.Trace( "  fstat failed, error %d\n", errno );
#endif

            update_a0_errno( cpu, result );
            break;
        }
        case SYS_gettimeofday:
        {
            tracer.Trace( "  rvos command SYS_gettimeofday\n" );
            linux_timeval * ptimeval = (linux_timeval *) cpu.getmem( cpu.regs[ RiscV::a0 ] );
            int result = 0;
            if ( 0 != ptimeval )
                result = gettimeofday( ptimeval );

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
            update_a0_errno( cpu, result );
            break;
        }
        case SYS_read:
        {
            int descriptor = (int) cpu.regs[ RiscV::a0 ];
            void * buffer = cpu.getmem( cpu.regs[ RiscV::a1 ] );
            unsigned buffer_size = (unsigned) cpu.regs[ RiscV::a2 ];
            tracer.Trace( "  rvos command SYS_read. descriptor %d, buffer %llx, buffer_size %u\n", descriptor, cpu.regs[ RiscV::a1 ], buffer_size );

            if ( 0 == descriptor ) //&& 1 == buffer_size )
            {
#ifdef _WIN32
                int r = g_consoleConfig.linux_getch();
#else
                int r = g_consoleConfig.portable_getch();
#endif
                * (char *) buffer = (char) r;
                cpu.regs[ RiscV::a0 ] = 1;
                tracer.Trace( "  getch read character %u == '%c'\n", r, printable( (uint8_t) r ) );
                break;
            }
            else if ( timebaseFrequencyDescriptor == descriptor && buffer_size >= 8 )
            {
                uint64_t freq = 1000000000000; // nanoseconds, but the LPi4a is off by 3 orders of magnitude, so I replicate that bug here
                memcpy( buffer, &freq, 8 );
                update_a0_errno( cpu, 8 );
                break;
            }

            int result = read( descriptor, buffer, buffer_size );
            update_a0_errno( cpu, result );
            break;
        }
        case SYS_write:
        {
            tracer.Trace( "  rvos command SYS_write. fd %lld, buf %llx, count %lld\n", cpu.regs[ RiscV::a0 ], cpu.regs[ RiscV::a1 ], cpu.regs[ RiscV::a2 ] );

            int descriptor = (int) cpu.regs[ RiscV::a0 ];
            uint8_t * p = (uint8_t *) cpu.getmem( cpu.regs[ RiscV::a1 ] );
            uint64_t count = cpu.regs[ RiscV::a2 ];

            if ( 0 == descriptor ) // stdin
            {
                errno = EACCES;
                update_a0_errno( cpu, -1 );
            }
            else
            {
                if ( 1 == descriptor || 2 == descriptor ) // stdout / stderr
                    tracer.Trace( "  writing '%.*s'\n", (int) count, p );

                tracer.TraceBinaryData( p, (uint32_t) count, 4 );
                size_t written = write( descriptor, p, (int) count );
                update_a0_errno( cpu, written );
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

#ifdef _WIN32
            flags = windows_translate_flags( flags );
#endif

            int descriptor = open( pname, flags, mode );
            tracer.Trace( "  descriptor: %d\n", descriptor );
            update_a0_errno( cpu, descriptor );
            break;
        }
        case SYS_close:
        {
            tracer.Trace( "  rvos command SYS_close\n" );
            int descriptor = (int) cpu.regs[ RiscV::a0 ];

            if ( descriptor >=0 && descriptor <= 3 )
            {
                // built-in handle stdin, stdout, stderr -- ignore
                cpu.regs[ RiscV::a0 ] = 0;
            }
            else
            {
                int result = 0;
#ifdef _WIN32
                if ( findFirstDescriptor == descriptor )
                {
                    if ( INVALID_HANDLE_VALUE != g_hFindFirst )
                    {
                        FindClose( g_hFindFirst );
                        g_hFindFirst = INVALID_HANDLE_VALUE;
                        g_acFindFirstPattern[ 0 ] = 0;
                    }
                }
                else if ( timebaseFrequencyDescriptor == descriptor )
                {
                    update_a0_errno( cpu, 0 );
                    break;
                }
                else
#else
    #ifndef OLDGCC      // the several-years-old Gnu C compiler for the RISC-V development boards
                if ( g_FindFirstDescriptor == descriptor )
                {
                    if ( 0 != g_FindFirst )
                    {
                        closedir( g_FindFirst );
                        g_FindFirst = 0;
                    }
                    g_FindFirstDescriptor = -1;
                    update_a0_errno( cpu, 0 );
                    break;
                }
    #endif
#endif
                result = close( descriptor );
                update_a0_errno( cpu, result );
            }
            break;
        }
        case SYS_getdents64:
        {
            int64_t result = 0;
            uint64_t descriptor = cpu.regs[ RiscV::a0 ];
            uint8_t * pentries = (uint8_t *) cpu.getmem( cpu.regs[ RiscV::a1 ] );
            uint64_t count = cpu.regs[ RiscV::a2 ];
            tracer.Trace( "  pentries: %p, count %llu\n", pentries, count );
            struct linux_dirent64_syscall * pcur = (struct linux_dirent64_syscall *) pentries;
            memset( pentries, 0, count );

#ifdef _WIN32
            if ( ( findFirstDescriptor != descriptor ) || ( 0 == g_acFindFirstPattern[ 0 ] ) )
            {
                tracer.Trace( "  getdents on unexpected descriptor or FindFirst (%p) not open\n", g_hFindFirst );
                errno = EBADF;
                update_a0_errno( cpu, -1 );
                break;
            }

            WIN32_FIND_DATAA fd = {0};

            if ( INVALID_HANDLE_VALUE == g_hFindFirst )
            {
                g_hFindFirst = FindFirstFileA( g_acFindFirstPattern, &fd );
                if ( INVALID_HANDLE_VALUE != g_hFindFirst )
                {
                    tracer.Trace( "  successfully opened FindFirst for pattern '%s'\n", g_acFindFirstPattern );

                    size_t len = strlen( fd.cFileName );
                    if ( len > ( count - sizeof( struct linux_dirent64_syscall ) ) )
                    {
                        errno = ENOENT;
                        result = -1;
                    }
                    else
                    {
                        pcur->d_ino = 100; // fake
                        tracer.Trace( "  len: %zd, sizeof struct %zd\n", len, sizeof( struct linux_dirent64_syscall ) );
                        size_t dname_off = offsetof( struct linux_dirent64_syscall, d_name );
                        pcur->d_reclen = (uint16_t) ( dname_off + len + 1 );
                        pcur->d_off = pcur->d_reclen;
                        strcpy( pcur->d_name, fd.cFileName );
                        if ( FILE_ATTRIBUTE_DIRECTORY & fd.dwFileAttributes )
                            pcur->d_type = 4;
                        else
                            pcur->d_type = 8;
                        tracer.Trace( "  wrote '%s' into the entry. d_reclen %d, d_off %d\n", pcur->d_name, pcur->d_reclen, pcur->d_off );
                        result = pcur->d_reclen;
                    }
                }
                else
                {
                    errno = ENOENT;
                    result = -1;
                }
            }
            else
            {
                // find next here

                BOOL found = FindNextFileA( g_hFindFirst, &fd );
                if ( found )
                {
                    size_t len = strlen( fd.cFileName );
                    if ( len > ( count - sizeof( struct linux_dirent64_syscall ) ) )
                    {
                        errno = ENOENT;
                        result = -1;
                    }
                    else
                    {
                        pcur->d_ino = 100; // fake
                        tracer.Trace( "  len: %zd, sizeof struct %zd\n", len, sizeof( struct linux_dirent64_syscall ) );
                        size_t dname_off = offsetof( struct linux_dirent64_syscall, d_name );
                        pcur->d_reclen = (uint16_t) ( dname_off + len + 1 );
                        pcur->d_off = pcur->d_reclen;
                        strcpy( pcur->d_name, fd.cFileName );

                        if ( FILE_ATTRIBUTE_DIRECTORY & fd.dwFileAttributes )
                            pcur->d_type = 4;
                        else
                            pcur->d_type = 8;
                        tracer.Trace( "  wrote '%s' into the entry. d_reclen %d, d_off %d\n", pcur->d_name, pcur->d_reclen, pcur->d_off );
                        result = pcur->d_reclen;
                    }
                }
                else
                {
                    tracer.Trace( "  out of next files\n" );
                    FindClose( g_hFindFirst );
                    g_hFindFirst = INVALID_HANDLE_VALUE;
                    g_acFindFirstPattern[ 0 ] = 0;
                    result = 0; // nothing left
                }
            }
#else
    #ifndef OLDGCC      // the several-years-old Gnu C compiler for the RISC-V development boards
            tracer.Trace( "  g_FindFirstDescriptor: %d, g_FindFirst: %p\n", g_FindFirstDescriptor, g_FindFirst );

            if ( -1 == g_FindFirstDescriptor )
            {
                g_FindFirstDescriptor = descriptor;
                g_FindFirst = fdopendir( descriptor );
            }

            if ( 0 == g_FindFirst )
            {
                errno = EBADF;
                g_FindFirstDescriptor = -1;
                update_a0_errno( cpu, -1 );
                break;
            }

            struct dirent * pent = readdir( g_FindFirst );
            if ( 0 != pent )
            {
                tracer.Trace( "  readdir returned '%s'\n", pent->d_name );
                size_t len = strlen( pent->d_name );    
                if ( len > ( count - sizeof( struct linux_dirent64_syscall ) ) )
                {
                    errno = ENOENT;
                    result = -1;
                }
                else
                {
                    pcur->d_ino = 100; // fake
                    tracer.Trace( "  len: %zd, sizeof struct %zd\n", len, sizeof( struct linux_dirent64_syscall ) );
                    size_t dname_off = offsetof( struct linux_dirent64_syscall, d_name );
                    tracer.Trace( "  d_name offset in the struct: %zd\n", dname_off );
                    pcur->d_reclen = dname_off + len + 1;
                    pcur->d_off = pcur->d_reclen;
                    strcpy( pcur->d_name, pent->d_name );

                    struct stat local_stat = {0};
                    result = stat( pent->d_name, & local_stat );

                    if ( S_ISDIR( local_stat.st_mode ) )
                        pcur->d_type = 4;
                    else
                        pcur->d_type = 8;

                    tracer.Trace( "  wrote '%s' into the entry. d_reclen %d, d_off %d\n", pcur->d_name, pcur->d_reclen, pcur->d_off );
                    result = pcur->d_reclen;
                }
            }
            else
            {
                tracer.Trace( "  readdir return 0, so there are no more files in the enumeration\n" );
                result = 0;
            }
    #endif
#endif
            update_a0_errno( cpu, result );
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
            fflush( stdout );
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
            errno = EACCES;
            update_a0_errno( cpu, -1 );
            break;
        }
#if !defined(OLDGCC) // the syscalls below aren't invoked from the old g++ compiler and runtime
        case SYS_openat:
        {
            tracer.Trace( "  rvos command SYS_openat\n" );
            // a0: directory. a1: asciiz string of file to open. a2: flags. a3: mode

            // G++ on RISC-V on a opendir() call will call this with 0x80000 set in flags. Ignored for now,
            // but this should call opendir() on non-Windows and on Windows would take substantial code to
            // implement since opendir() doesn't exist. For now it fails with errno 13.

            int directory = (int) cpu.regs[ RiscV::a0 ];
#ifdef __APPLE__
            if ( -100 == directory )
                directory = -2; // Linux vs. MacOS
#endif
            const char * pname = (const char *) cpu.getmem( cpu.regs[ RiscV::a1 ] );
            int flags = (int) cpu.regs[ RiscV::a2 ];
            int mode = (int) cpu.regs[ RiscV::a3 ];
            int64_t descriptor = 0;

            tracer.Trace( "  open dir %d, flags %x, mode %x, file '%s'\n", directory, flags, mode, pname );

            if ( !strcmp( pname, "/proc/device-tree/cpus/timebase-frequency" ) )
            {
                descriptor = timebaseFrequencyDescriptor;
                update_a0_errno( cpu, descriptor );
                break;
            }

#ifdef _WIN32
            flags = windows_translate_flags( flags );

            // bugbug: directory ignored and assumed to be local (-100)

            strcpy( acPath, pname );
            slash_to_backslash( acPath );

            DWORD attr = GetFileAttributesA( acPath );
            if ( ( INVALID_FILE_ATTRIBUTES != attr ) && ( attr & FILE_ATTRIBUTE_DIRECTORY ) )
            {
                if ( INVALID_HANDLE_VALUE != g_hFindFirst )
                {
                    FindClose( g_hFindFirst );
                    g_hFindFirst = INVALID_HANDLE_VALUE;
                }
                strcpy( g_acFindFirstPattern, acPath );
                strcat( g_acFindFirstPattern, "\\*.*" );
                descriptor = findFirstDescriptor;
            }
            else
                descriptor = _open( pname, flags, mode );
#else
#if defined(__ARM_32BIT_STATE) || defined(__ARM_64BIT_STATE)
            flags = linux_riscv64_to_arm_open_flags( flags );
#endif
            descriptor = openat( directory, pname, flags, mode );
#endif
            update_a0_errno( cpu, descriptor );
            break;
        }
        case SYS_sysinfo:
        {
#if defined(_WIN32) || defined(__APPLE__)
            errno = EACCES;
            update_a0_errno( cpu, -1 );
#else
            int result = sysinfo( (struct sysinfo *) cpu.getmem( cpu.regs[ RiscV::a0 ] ) );
            update_a0_errno( cpu, result );
#endif
            break;
        }
        case SYS_newfstatat:
        {
            const char * path = (char *) cpu.getmem( cpu.regs[ RiscV::a1 ] );
            tracer.Trace( "  rvos command SYS_newfstatat, id %lld, path '%s', flags %llx\n", cpu.regs[ RiscV::a0 ], path, cpu.regs[ RiscV::a3 ] );
            int descriptor = (int) cpu.regs[ RiscV::a0 ];
            int result = 0;

#ifdef _WIN32
            // ignore the folder argument on Windows
            struct stat_linux_syscall local_stat = {0};
            result = fill_pstat_windows( descriptor, & local_stat, path );
            if ( 0 == result )
            {
                size_t cbStat = sizeof( struct stat_linux_syscall );
                tracer.Trace( "  sizeof stat_linux_syscall: %zd\n", cbStat );
                assert( 128 == cbStat );  // 128 is the size of the stat struct this syscall on RISC-V Linux
                memcpy( cpu.getmem( cpu.regs[ RiscV::a2 ] ), & local_stat, cbStat );
                tracer.Trace( "  file size in bytes: %zd, offsetof st_size: %zd\n", local_stat.st_size, offsetof( struct stat_linux_syscall, st_size ) );
            }
            else
                tracer.Trace( "  fill_pstat_windows failed\n" );
#else
            tracer.Trace( "  sizeof struct stat: %zd\n", sizeof( struct stat ) );
            struct stat local_stat = {0};
            struct stat_linux_syscall local_stat_syscall = {0};
            int flags = (int) cpu.regs[ RiscV::a3 ];
            tracer.Trace( "  flag AT_SYMLINK_NOFOLLOW: %llx, flags %x", AT_SYMLINK_NOFOLLOW, flags );
#ifdef __APPLE__
            if ( -100 == descriptor ) // current directory
                descriptor = -2;
            if ( 0x100 == flags ) // AT_SYMLINK_NOFOLLOW
                flags = 0x20; // Apple's value for this flag
            else
                flags = 0; // no other flags are supported on MacOS
            tracer.Trace( "  translated flags for MacOS: %x\n", flags );
            if ( 0 == path[ 0 ] )
                result = fstat( descriptor, & local_stat );
            else
                result = fstatat( descriptor, path, & local_stat, flags );
#else            
            result = fstatat( descriptor, path, & local_stat, flags );
#endif
            if ( 0 == result )
            {
                // the syscall version of stat has similar fields but a different layout, so copy fields one by one

                struct stat_linux_syscall * pout = (struct stat_linux_syscall *) cpu.getmem( cpu.regs[ RiscV::a2 ] );
                pout->st_dev = local_stat.st_dev;
                pout->st_ino = local_stat.st_ino;
                pout->st_mode = local_stat.st_mode;
                pout->st_nlink = local_stat.st_nlink;
                pout->st_uid = local_stat.st_uid;
                pout->st_gid = local_stat.st_gid;
                pout->st_rdev = local_stat.st_rdev;
                pout->st_size = local_stat.st_size;
                pout->st_blksize = local_stat.st_blksize;
                pout->st_blocks = local_stat.st_blocks;
#ifdef __APPLE__
                pout->st_atim = local_stat.st_atimespec;
                pout->st_mtim = local_stat.st_mtimespec;
                pout->st_ctim = local_stat.st_ctimespec;
#else
                pout->st_atim = local_stat.st_atim;
                pout->st_mtim = local_stat.st_mtim;
                pout->st_ctim = local_stat.st_ctim;
#endif
                tracer.Trace( "  file size %zd, isdir %s\n", local_stat.st_size, S_ISDIR( local_stat.st_mode ) ? "yes" : "no" );
            }
#endif
            update_a0_errno( cpu, result );
            break;
        }
        case SYS_chdir:
        {
            const char * path = (const char *) cpu.getmem( cpu.regs[ RiscV::a0 ] );
            tracer.Trace( "  rvos command SYS_chdir path %s\n", path );
#ifdef _WIN32
            int result = _chdir( path );
#else
            int result = chdir( path );
#endif
            update_a0_errno( cpu, result );
            break;
        }
        case SYS_mkdirat:
        {
            int directory = (int) cpu.regs[ RiscV::a0 ];
            const char * path = (const char *) cpu.getmem( cpu.regs[ RiscV::a1 ] );
#ifdef _WIN32 // on windows ignore the directory and mode arguments
            tracer.Trace( "  rvos command SYS_mkdirat path %s\n", path );
            int result = _mkdir( path );
#else
#ifdef __APPLE__
            if ( -100 == directory )
                directory = -2;
#endif     
            mode_t mode = (mode_t) cpu.regs[ RiscV::a2 ];
            tracer.Trace( "  rvos command SYS_mkdirat dir %d, path %s, mode %x\n", directory, path, mode );
            int result = mkdirat( directory, path, mode );
#endif
            update_a0_errno( cpu, result );
            break;
        }
        case SYS_unlinkat:
        {
            int directory = (int) cpu.regs[ RiscV::a0 ];
            const char * path = (const char *) cpu.getmem( cpu.regs[ RiscV::a1 ] );
            int flags = (int) cpu.regs[ RiscV::a2 ];
            tracer.Trace( "  rvos command SYS_unlinkat dir %d, path %s, flags %x\n", directory, path, flags );
#ifdef _WIN32 // on windows ignore the directory and flags arguments
            DWORD attr = GetFileAttributesA( path );
            int result = 0;
            if ( INVALID_FILE_ATTRIBUTES != result )
            {
                if ( attr & FILE_ATTRIBUTE_DIRECTORY )
                    result = _rmdir( path ); // unlink will fail with error 13 "permission denied", so use rmdir instead
                else
                    result = remove( path );
            }
#else
#ifdef __APPLE__
            if ( -100 == directory )
                directory = -2;
#endif     
            int result = unlinkat( directory, path, flags );
#endif
            update_a0_errno( cpu, result );
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
                cpu.regs[ RiscV::a0 ] = (uint64_t) -1; // fail this until/unless there is a real-world use
            break;
        }
        case SYS_writev:
        {
            int descriptor = (int) cpu.regs[ RiscV::a0 ];
            const struct iovec * pvec = (const struct iovec *) cpu.getmem( cpu.regs[ RiscV::a1 ] );
            if ( 1 == descriptor || 2 == descriptor )
                tracer.Trace( "  desc %d: writing '%.*s'\n", descriptor, pvec->iov_len, cpu.getmem( (uint64_t) pvec->iov_base ) );

#ifdef _WIN32
            int64_t result = write( descriptor, cpu.getmem( (uint64_t) pvec->iov_base ), (unsigned) pvec->iov_len );
#else
            struct iovec vec_local;
            vec_local.iov_base = cpu.getmem( (uint64_t) pvec->iov_base );
            vec_local.iov_len = pvec->iov_len;
            tracer.Trace( "  write length: %u to descriptor %d at addr %p\n", pvec->iov_len, descriptor, vec_local.iov_base );
            int64_t result = writev( descriptor, &vec_local, cpu.regs[ RiscV::a2 ] );
#endif
            update_a0_errno( cpu, result );
            break;
        }
        case SYS_clock_gettime:
        {
            clockid_t cid = (clockid_t) cpu.regs[ RiscV::a0 ];
            struct timespec_syscall * ptimespec = (struct timespec_syscall *) cpu.getmem( cpu.regs[ RiscV::a1 ] );
            #ifdef __APPLE__ // Linux vs MacOS
                if ( 1 == cid )
                    cid = CLOCK_REALTIME;
                else if ( 5 == cid )
                    cid = CLOCK_REALTIME;
            #endif

#ifdef _WIN32
            int result = msc_clock_gettime( cid, ptimespec );
#else
            struct timespec local_ts; // this varies in size from platform to platform
            int result = clock_gettime( cid, & local_ts );
            ptimespec->tv_sec = local_ts.tv_sec;
            ptimespec->tv_nsec = local_ts.tv_nsec;
#endif
            update_a0_errno( cpu, result );
            break;
        }
        case SYS_fdatasync:
        {
            errno = EACCES;
            update_a0_errno( cpu, -1 );
            break;
        }
        case SYS_sigaction:
        {
            errno = EACCES;
            update_a0_errno( cpu, -1 );
            break;
        }
        case SYS_rt_sigprocmask:
        {
            errno = EACCES;
            update_a0_errno( cpu, -1 );
            break;
        }
        case SYS_prctl:
        {
            update_a0_errno( cpu, 0 );
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
        case SYS_renameat2:
        {
            int olddirfd = (int) cpu.regs[ RiscV::a0 ];
            const char * oldpath = (const char *) cpu.getmem( cpu.regs[ RiscV::a1 ] );
            int newdirfd = (int) cpu.regs[ RiscV::a2 ];
            const char * newpath = (const char *) cpu.getmem( cpu.regs[ RiscV::a3 ] );
            unsigned int flags = (unsigned int) cpu.regs[ RiscV::a4 ];
            tracer.Trace( "  renaming '%s' to '%s'\n", oldpath, newpath );
#ifdef _WIN32
            int result = rename( oldpath, newpath );
#elif defined( __APPLE__ )
            if ( -100 == olddirfd )
                olddirfd = -2;
            if ( -100 == newdirfd )
                newdirfd = -2;
            int result = renameat( olddirfd, oldpath, newdirfd, newpath ); // macos has no renameat2s
#else
            int result = renameat2( olddirfd, oldpath, newdirfd, newpath, flags );
#endif
            update_a0_errno( cpu, result );
            break;
        }
        case SYS_getrandom:
        {
            void * buf = cpu.getmem( cpu.regs[ RiscV::a0 ] );
            size_t buflen = cpu.regs[ RiscV::a1 ];
            unsigned int flags = (unsigned int) cpu.regs[ RiscV::a2 ];
            int64_t result = 0;

#if defined(_WIN32) || defined(__APPLE__)
            int * pbuf = (int *) buf;
            size_t count = buflen / sizeof( int );
            for ( size_t i = 0; i < count; i++ )
                pbuf[ i ] = (int) rand64();
            result = buflen;
#else
            result = getrandom( buf, buflen, flags );
#endif
            update_a0_errno( cpu, result );
            break;
        }
        case SYS_riscv_flush_icache :
        {
            cpu.regs[ RiscV::a0 ] = 0;
            break;
        }
        case SYS_pselect6:
        {
            int nfds = (int) cpu.regs[ RiscV::a0 ];
            void * readfds = (void *) cpu.regs[ RiscV::a1 ];

            if ( 1 == nfds && 0 != readfds )
            {
                // check to see if stdin has a keystroke available

                cpu.regs[ RiscV::a0 ] = g_consoleConfig.portable_kbhit();
                tracer.Trace( "  pselect6 keystroke available on stdin: %llx\n", cpu.regs[ RiscV::a0 ] );
            }
            else
            {
                // lie and say no I/O is ready

                cpu.regs[ RiscV::a0 ] = 0;
            }

            break;
        }
        case SYS_readlinkat:
        {
            int dirfd = (int) cpu.regs[ RiscV::a0 ];
            const char * pathname = (const char *) cpu.getmem( cpu.regs[ RiscV::a1 ] );
            char * buf = (char *) cpu.getmem( cpu.regs[ RiscV::a2 ] );
            size_t bufsiz = (size_t) cpu.regs[ RiscV::a3 ];
            tracer.Trace( "  readlinkat pathname %p == '%s', buf %p, bufsiz %zd, dirfd %d\n", pathname, pathname, buf, bufsiz, dirfd );
            int64_t result = -1;

#ifdef _WIN32
            errno = EINVAL; // no symbolic links on Windows as far as this emulator is concerned
            result = -1;
#else
            result = readlinkat( dirfd, pathname, buf, bufsiz );
            tracer.Trace( "  result of readlinkat(): %d\n", result );
#endif

            update_a0_errno( cpu, result );
            break;
        }
        case SYS_ioctl:
        {
#ifndef _WIN32 // kbhit() works without all this fuss on Windows
            int fd = (int) cpu.regs[ RiscV::a0 ];
            unsigned long request = (unsigned long) cpu.regs[ RiscV::a1 ];
            tracer.Trace( "  ioctl fd %d, request %lx\n", fd, request );
            struct local_kernel_termios * pt = (struct local_kernel_termios *) cpu.getmem( cpu.regs[ RiscV::a2 ] );

            if ( 0 == fd )
            {
                // likely a TCGETS or TCSETS on stdin to check or enable non-blocking reads for a keystroke

                if ( 0x5401 == request ) // TCGETS
                {
                    struct termios val;
                    tcgetattr( 0, &val );
                    tracer.Trace( "  iflag %#x, oflag %#x, cflag %#x, lflag %#x\n", val.c_iflag, val.c_oflag, val.c_cflag, val.c_lflag );
                    pt->c_iflag = val.c_iflag;
                    pt->c_oflag = val.c_oflag;
                    pt->c_cflag = val.c_cflag;
                    pt->c_lflag = val.c_lflag;
#ifdef __APPLE__
                    pt->c_iflag = map_termios_iflag_macos_to_linux( pt->c_iflag );
                    pt->c_oflag = map_termios_oflag_macos_to_linux( pt->c_oflag );
                    pt->c_cflag = map_termios_cflag_macos_to_linux( pt->c_cflag );
                    pt->c_lflag = map_termios_lflag_macos_to_linux( pt->c_lflag );
                    tracer.Trace( "  translated iflag %#x, oflag %#x, cflag %#x, lflag %#x\n", pt->c_iflag, pt->c_oflag, pt->c_cflag, pt->c_lflag );
#else
                    pt->c_line = val.c_line;
                    memcpy( & pt->c_cc, & val.c_cc, get_min( sizeof( pt->c_cc ), sizeof( val.c_cc ) ) );
#endif
                    tracer.Trace( "  ioctl queried termios on stdin, sizeof local_kernel_termios %zd, sizeof val %zd\n", 
                                sizeof( struct local_kernel_termios ), sizeof( val ) );
                }
                else if ( 0x5402 == request ) // TCSETS
                {
                    struct termios val;
                    memset( &val, 0, sizeof val );
                    tracer.TraceBinaryData( (uint8_t *) pt, sizeof( struct local_kernel_termios ), 4 );
                    tracer.Trace( "  oflag %#x OPOST %#x ONLCR %#x OCRNL %#x ONOCR %#x ONLRET %#x\n",
                                  pt->c_oflag, OPOST, ONLCR, OCRNL, ONOCR, ONLRET );
                    tracer.Trace( "  iflag %#x IGNBRK %#x BRKINT %#x IGNPAR %#x PARMRK %#x INPCK %#x ISTRIP %#x INLCR %#x IGNCR %#x ICRNL %#x IXON %#x IXOFF %#x IXANY %#x IMAXBEL %#x IUTF8 %#x\n",
                                  pt->c_iflag, IGNBRK, BRKINT, IGNPAR, PARMRK, INPCK, ISTRIP, INLCR, IGNCR, ICRNL, IXON, IXOFF, IXANY, IMAXBEL, IUTF8 );
                    tracer.Trace( "  cflag %#x CSIZE %#x, CSTOPB %#x CREAD %#x PARENB %#x PARODD %#x CS5 %#x CS6 %#x CS7 %#x CS8 %#x HUPCL %#x CLOCAL %#x\n",
                                  CSIZE, CSTOPB, CREAD, PARENB, PARODD, CS5, CS6, CS7, CS8, HUPCL, CLOCAL );
                    tracer.Trace( "  lflag %#x ECHOKE %#x ECHOE %#x ECHOK %#x ECHO %#x ECHONL %#x ECHOPRT %#x ECHOCTL %#x ICANON %#x ISIG %#x IEXTEN %#x EXTPROC %#x TOSTOP %#x\n",
                                  ECHOKE, ECHOE, ECHOK, ECHO, ECHONL, ECHOPRT, ECHOCTL, ICANON, ISIG, IEXTEN, EXTPROC, TOSTOP );

                    val.c_iflag = pt->c_iflag;
                    val.c_oflag = pt->c_oflag;
                    val.c_cflag = pt->c_cflag;
                    val.c_lflag = pt->c_lflag;
                    tracer.Trace( "  iflag %#x, oflag %#x, cflag %#x, lflag %#x\n", val.c_iflag, val.c_oflag, val.c_cflag, val.c_lflag );                   
#ifdef __APPLE__
                    val.c_iflag = map_termios_iflag_linux_to_macos( val.c_iflag );
                    val.c_oflag = map_termios_oflag_linux_to_macos( val.c_oflag );
                    val.c_cflag = map_termios_cflag_linux_to_macos( val.c_cflag );
                    val.c_lflag = map_termios_lflag_linux_to_macos( val.c_lflag );
                    tracer.Trace( "  translated iflag %#x, oflag %#x, cflag %#x, lflag %#x\n", val.c_iflag, val.c_oflag, val.c_cflag, val.c_lflag );
#else
                    val.c_line = pt->c_line;
                    memcpy( & val.c_cc, & pt->c_cc, get_min( sizeof( pt->c_cc ), sizeof( val.c_cc ) ) );
#endif
                    tracer.TraceBinaryData( (uint8_t *) &val, sizeof( struct termios ), 4 );
                    tcsetattr( 0, TCSANOW, &val );
                    tracer.Trace( "  ioctl set termios on stdin\n" );
                }
            }
#endif
            cpu.regs[ RiscV::a0 ] = 0;
            break;
        }
        case SYS_set_tid_address:
        case SYS_set_robust_list:
        case SYS_prlimit64:
        case SYS_mprotect:
            // ignore for now
            break;

#endif // !defined(OLDGCC)
        default:
        {
            printf( "error; ecall invoked with unknown command %llu = %llx, a0 %#llx, a1 %#llx, a2 %#llx\n",
                    cpu.regs[ RiscV::a7 ], cpu.regs[ RiscV::a7 ], cpu.regs[ RiscV::a0 ], cpu.regs[ RiscV::a1 ], cpu.regs[ RiscV::a2 ] );
            tracer.Trace( "error; ecall invoked with unknown command %llu = %llx, a0 %#llx, a1 %#llx, a2 %#llx\n",
                          cpu.regs[ RiscV::a7 ], cpu.regs[ RiscV::a7 ], cpu.regs[ RiscV::a0 ], cpu.regs[ RiscV::a1 ], cpu.regs[ RiscV::a2 ] );
            fflush( stdout );
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

void riscv_hard_termination( RiscV & cpu, const char *pcerr, uint64_t error_value )
{
    g_consoleConfig.RestoreConsole( false );

    tracer.Trace( "rvos (%s) fatal error: %s %llx\n", target_platform(), pcerr, error_value );
    printf( "rvos (%s) fatal error: %s %llx\n", target_platform(), pcerr, error_value );

    tracer.Trace( "pc: %llx %s\n", cpu.pc, riscv_symbol_lookup( cpu.pc ) );
    printf( "pc: %llx %s\n", cpu.pc, riscv_symbol_lookup( cpu.pc ) );

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

    tracer.Trace( "  %s\n", build_string() );
    printf( "  %s\n", build_string() );

    tracer.Flush();
    fflush( stdout );
    exit( 1 );
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

const char * riscv_symbol_lookup( uint64_t address )
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
    {
        printf( "can't open elf image file: %s\n", pimage );
        usage();
    }

    CFile file( fp );
    size_t read = fread( &ehead, sizeof ehead, 1, fp );

    if ( 1 != read )
        usage( "elf image file is invalid" );

    if ( 0x464c457f != ehead.magic )
        usage( "elf image file's magic header is invalid" );

    if ( 0xf3 != ehead.machine )
        usage( "elf image isn't for RISC-V" );

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
        tracer.Trace( "  memory size: %llx\n", head.memory_size );
        tracer.Trace( "  alignment: %llx\n", head.alignment );

        if ( 2 == head.type )
        {
            printf( "dynamic linking is not supported by rvos. link with -static\n" );
            exit( 1 );
        }

        uint64_t just_past = head.physical_address + head.memory_size;
        if ( just_past > memory_size )
            memory_size = just_past;

        if ( ( 0 != head.physical_address ) && ( ( 0 == g_base_address ) || g_base_address > head.physical_address ) )
            g_base_address = head.physical_address;
    }

    memory_size -= g_base_address;
    tracer.Trace( "memory_size of content to load from elf file: %llx\n", memory_size );

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

    // load the symbol data into RAM

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

    // void out the entries that don't have symbol names or have mangled names that start with $

    for ( size_t se = 0; se < g_symbols.size(); se++ )
        if ( ( 0 == g_symbols[se].name ) || ( '$' == g_string_table[ g_symbols[se].name ] ) )
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

    // Find the "tracenow" variable in the app, which can be used to dynamically control instruction tracing

    for ( size_t se = 0; se < g_symbols.size(); se++ )
    {
        if ( !strcmp( "g_TRACENOW", & g_string_table[ g_symbols[ se ].name ] ) )
            g_ptracenow = (bool *) ( memory.data() + g_symbols[ se ].value - g_base_address );
    }

    // load the program into RAM

    for ( uint16_t ph = 0; ph < ehead.program_header_table_entries; ph++ )
    {
        size_t o = ehead.program_header_table + ( ph * ehead.program_header_table_size );
        ElfProgramHeader64 head = {0};
        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, 1, get_min( sizeof( head ), (size_t) ehead.program_header_table_size ), fp );

        // head.type 1 == load. Other entries will overlap and even have physical addresses, but they are redundant

        if ( 0 != head.file_size && 0 != head.physical_address && 1 == head.type )
        {
            fseek( fp, (long) head.offset_in_image, SEEK_SET );
            read = fread( memory.data() + head.physical_address - g_base_address, 1, head.file_size, fp );
            if ( 0 == read )
                usage( "can't read image" );

            tracer.Trace( "  read type %s: %llx bytes into physical address %llx - %llx\n", head.show_type(), head.file_size,
                          head.physical_address, head.physical_address + head.memory_size - 1 );
            tracer.TraceBinaryData( memory.data() + head.physical_address - g_base_address, get_min( (uint32_t) head.file_size, (uint32_t) 128 ), 4 );
        }
    }

    // write the command-line arguments into the vm memory in a place where _start can find them.
    // there's an array of pointers to the args followed by the arg strings at offset arg_data.

    uint64_t * parg_data = (uint64_t *) ( memory.data() + arg_data );
    const uint32_t max_args = 40;
    char * buffer_args = (char *) & ( parg_data[ max_args ] );
    size_t image_len = strlen( pimage );
    vector<char> full_command( 2 + image_len + strlen( app_args ) );
    strcpy( full_command.data(), pimage );
    backslash_to_slash( full_command.data() );
    full_command[ image_len ] = ' ';
    strcpy( full_command.data() + image_len + 1, app_args );

    strcpy( buffer_args, full_command.data() );
    char * pargs = buffer_args;
    size_t args_len = strlen( buffer_args );

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

    uint64_t env_offset = args_len + 1;
    char * penv_data = (char *) ( buffer_args + env_offset );
    strcpy( penv_data, "OS=RVOS" );
    tracer.Trace( "args_len %d, penv_data %p\n", args_len, penv_data );
    tracer.Trace( "env data block: at vm address %llx\n", ( penv_data - (char *) memory.data() ) + g_base_address );
    tracer.TraceBinaryData( (uint8_t *) ( memory.data() + arg_data ), g_args_commit + 0x20, 4 );

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

    if ( 1 == ( 1 & app_argc ) )
        pstack--;

    pstack -= 2; // the AT_NULL record will be here since memory is initialized to 0

    pstack -= 2;
    AuxProcessStart * paux = (AuxProcessStart *) pstack;    
    paux->a_type = 25; // AT_RANDOM
    paux->a_un.a_val = prandom;

    pstack--; // end of environment data is 0

    pstack--; // move to where the one environment variable is set OS=RVOS
    *pstack = (uint64_t) ( env_offset + arg_data + g_base_address + max_args * sizeof( uint64_t ) );
    tracer.Trace( "the OS environment argument is at VM address %llx\n", *pstack );

    pstack--; // the last argv is 0 to indicate the end

    for ( int iarg = (int) app_argc - 1; iarg >= 0; iarg-- )
    {
        pstack--;
        *pstack = parg_data[ iarg ];
    }

    pstack--;
    *pstack = app_argc;

    uint32_t to_show =  (uint32_t) ( (uint64_t) ( memory.data() + memory_size ) - (uint64_t) pstack );
    tracer.Trace( "stack at start (beginning with argc) -- %llu bytes:\n", to_show );
    tracer.TraceBinaryData( (uint8_t *) pstack, to_show, 2 );

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
        printf( "image isn't 64-bit (2), it's %u\n", ehead.bit_width );
        return;
    }

    printf( "header fields:\n" );
    printf( "  bit_width: %u\n", ehead.bit_width );
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
        printf( "  memory size: %llx\n", head.memory_size );
        printf( "  alignment: %llx\n", head.alignment );

        uint64_t just_past = head.physical_address + head.memory_size;
        if ( just_past > memory_size )
            memory_size = just_past;

        if ( 0 != head.physical_address )
        {
            if ( ( 0 == g_base_address ) || ( g_base_address > head.physical_address ) )
                g_base_address = head.physical_address;
        }
    }

    memory_size -= g_base_address;

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

        if ( ( 0 == pcApp ) && ( '-' == c
#if defined( WATCOM ) || defined( _WIN32 )
            || '/' == c
#endif
           ) )
        {
            char ca = (char) tolower( parg[1] );

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

    g_consoleConfig.EstablishConsoleOutput( 0, 0 );

    if ( generateRVCTable )
    {
        bool ok = RiscV::generate_rvc_table( "rvctable.txt" );
        if ( ok )
            printf( "rvctable.txt successfully created\n" );
        else
            printf( "unable to create rvctable.txt\n" );

        g_consoleConfig.RestoreConsole( false );
        return 0;
    }

    if ( 0 == pcApp )
    {
        usage( "no executable specified\n" );
        assume_false;
    }

    strcpy( acApp, pcApp );
    bool appExists = file_exists( acApp );
    if ( !appExists )
    {
        if ( !ends_with( acApp, ".elf" ) )
        {
            strcat( acApp, ".elf" );
            appExists = file_exists( acApp );
        }
    }

    if ( !appExists )
        usage( "input .elf executable file not found" );

    if ( elfInfo )
    {
        elf_info( acApp );
        g_consoleConfig.RestoreConsole( false );
        return 0;
    }

    bool ok = load_image( acApp, acAppArgs );
    if ( ok )
    {
        unique_ptr<RiscV> cpu( new RiscV( memory, g_base_address, g_execution_address, g_compressed_rvc, g_stack_commit, g_top_of_stack ) );
        cpu->trace_instructions( traceInstructions );
        uint64_t cycles = 0;

        #ifdef _WIN32
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

    g_consoleConfig.RestoreConsole( false );
    tracer.Shutdown();

    return g_exit_code;
} //main
