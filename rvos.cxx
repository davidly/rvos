/*
#ifdef ARMOS
    This emulates an extemely simple ARM64 Linux-like OS.
    It can load and execute ARM64 apps built with Gnu tools in .elf files targeting Linux.
    Written by David Lee in October 2024
#else // RVOS
    This emulates an extemely simple RISC-V OS.
    Per https://riscv.org/wp-content/uploads/2017/05/riscv-privileged-v1.10.pdf
    It's an AEE (Application Execution Environment) that exposes an ABI (Application Binary Inferface) for an Application.
    It runs in RISC-V M mode, similar to embedded systems.
    It can load and execute 64-bit RISC-V apps built with Gnu tools in .elf files targeting Linux.
    Written by David Lee in February 2023
    Useful: https://github.com/jart/cosmopolitan/blob/1.0/libc/sysv/consts.sh
#endif
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
#include <locale.h>

#include <djl_os.hxx>

#ifdef _WIN32
    #include <io.h>

    struct iovec
    {
       void * iov_base; /* Starting address */
       size_t iov_len; /* Length in bytes */
    };

    struct tms
    {
        uint64_t tms_utime;
        uint64_t tms_stime;
        uint64_t tms_cutime;
        uint64_t tms_cstime;
    };

    typedef SSIZE_T ssize_t;

    #define local_KERNEL_NCCS 19
    struct local_kernel_termios
    {
        uint32_t c_iflag;     /* input mode flags */
        uint32_t c_oflag;     /* output mode flags */
        uint32_t c_cflag;     /* control mode flags */
        uint32_t c_lflag;     /* local mode flags */
        uint8_t c_line;       /* line discipline */
        uint8_t c_cc[local_KERNEL_NCCS]; /* control characters */
    };
#else
    #include <unistd.h>

    #ifndef OLDGCC      // the several-years-old Gnu C compiler for the RISC-V development boards
        #include <termios.h>
        #include <sys/random.h>
        #include <sys/uio.h>
        #include <sys/times.h>
        #include <sys/resource.h>
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
#include <djl_mmap.hxx>

using namespace std;
using namespace std::chrono;

#include "linuxem.h"

#ifdef ARMOS

    #include "arm64.hxx"

    #define CPUClass Arm64
    #define ELF_MACHINE_ISA 0xb7
    #define APP_NAME "ARMOS"
    #define LOGFILE_NAME L"armos.log"

    #define REG_SYSCALL 8
    #define REG_RESULT 0
    #define REG_ARG0 0
    #define REG_ARG1 1
    #define REG_ARG2 2
    #define REG_ARG3 3
    #define REG_ARG4 4
    #define REG_ARG5 5

#elif defined( RVOS )

    #include "riscv.hxx"

    #define CPUClass RiscV
    #define ELF_MACHINE_ISA 0xf3
    #define APP_NAME "RVOS"
    #define LOGFILE_NAME L"rvos.log"

    #define REG_SYSCALL RiscV::a7
    #define REG_RESULT RiscV::a0
    #define REG_ARG0 RiscV::a0
    #define REG_ARG1 RiscV::a1
    #define REG_ARG2 RiscV::a2
    #define REG_ARG3 RiscV::a3
    #define REG_ARG4 RiscV::a4
    #define REG_ARG5 RiscV::a5

#else

    #error "One of ARMOS or RVOS must be defined for compilation"

#endif

CDJLTrace tracer;
ConsoleConfiguration g_consoleConfig;
bool g_compressed_rvc = false;                 // is the app compressed risc-v?
const uint64_t g_args_commit = 1024;           // storage spot for command-line arguments and environment variables
const uint64_t g_stack_commit = 128 * 1024;    // RAM to allocate for the fixed stack
uint64_t g_brk_commit = 40 * 1024 * 1024;      // RAM to reserve if the app calls brk to allocate space. 40 meg default
uint64_t g_mmap_commit = 40 * 1024 * 1024;     // RAM to reserve if the app mmap to allocate space. 40 meg default
bool g_terminate = false;                      // has the app asked to shut down?
int g_exit_code = 0;                           // exit code of the app in the vm
vector<uint8_t> memory;                        // RAM for the vm
uint64_t g_base_address = 0;                   // vm address of start of memory
uint64_t g_execution_address = 0;              // where the program counter starts
uint64_t g_brk_offset = 0;                     // offset of brk, initially g_end_of_data
uint64_t g_mmap_offset = 0;                    // offset of where mmap allocations start
uint64_t g_highwater_brk = 0;                  // highest brk seen during app; peak dynamically-allocated RAM
uint64_t g_end_of_data = 0;                    // official end of the loaded app
uint64_t g_bottom_of_stack = 0;                // just beyond where brk might move
uint64_t g_top_of_stack = 0;                   // argc, argv, penv, aux records sit above this
CMMap g_mmap;                                  // for mmap and munmap system calls

// fake descriptors.
// /etc/timezone is not implemented, so apps running in the emulator on Windows assume UTC

const uint64_t findFirstDescriptor = 3000;
const uint64_t timebaseFrequencyDescriptor = 3001;
const uint64_t osreleaseDescriptor = 3002;

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

struct linux_timeval
{
    uint64_t tv_sec;       // time_t
    uint64_t tv_usec;      // suseconds_t
};

struct linux_tms_syscall
{
    uint64_t tms_utime;
    uint64_t tms_stime;
    uint64_t tms_cutime;
    uint64_t tms_cstime;
};

struct linux_rusage_syscall {
    struct linux_timeval ru_utime; /* user CPU time used */
    struct linux_timeval ru_stime; /* system CPU time used */
    long   ru_maxrss;        /* maximum resident set size */
    long   ru_ixrss;         /* integral shared memory size */
    long   ru_idrss;         /* integral unshared data size */
    long   ru_isrss;         /* integral unshared stack size */
    long   ru_minflt;        /* page reclaims (soft page faults) */
    long   ru_majflt;        /* page faults (hard page faults) */
    long   ru_nswap;         /* swaps */
    long   ru_inblock;       /* block input operations */
    long   ru_oublock;       /* block output operations */
    long   ru_msgsnd;        /* IPC messages sent */
    long   ru_msgrcv;        /* IPC messages received */
    long   ru_nsignals;      /* signals received */
    long   ru_nvcsw;         /* voluntary context switches */
    long   ru_nivcsw;        /* involuntary context switches */
};

struct pollfd_syscall {
    int fd;
    short events;
    short revents;
};

struct timespec_syscall {
    uint64_t tv_sec;
    uint64_t tv_nsec;
};

#define SYS_NMLN 65 // appears to be true for Arm64.

struct utsname_syscall {

    /** The information returned by uname(). */
    /** The OS name. "Linux" on Android. */
    char sysname[SYS_NMLN];

    /** The name on the network. Typically "localhost" on Android. */
    char nodename[SYS_NMLN];

    /** The OS release. Typically something like "4.4.115-g442ad7fba0d" on Android. */
    char release[SYS_NMLN];

    /** The OS version. Typically something like "#1 SMP PREEMPT" on Android. */
    char version[SYS_NMLN];

    /** The hardware architecture. Typically "aarch64" on Android. */
    char machine[SYS_NMLN];

    /** The domain name set by setdomainname(). Typically "localdomain" on Android. */
    char domainname[SYS_NMLN];
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
            return "relocation entries with addends";
        if ( 5 == basetype )
            return "symbol hash table";
        if ( 6 == basetype )
            return "dynamic";
        if ( 7 == basetype )
            return "note";
        if ( 8 == basetype )
            return "nobits";
        if ( 9 == basetype )
            return "relocation entries without addends";
        if ( 10 == basetype )
            return "shlib";
        if ( 11 == basetype )
            return "dynsym";
        if ( 12 == basetype )
            return "num";
        if ( 14 == basetype )
            return "initialization functions";
        if ( 15 == basetype )
            return "termination functions";
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

void usage( char const * perror = 0 )
{
    g_consoleConfig.RestoreConsole( false );

    if ( 0 != perror )
        printf( "error: %s\n", perror );

    printf( "usage: %s <%s arguments> <elf_executable> <app arguments>\n", APP_NAME, APP_NAME );
    printf( "   arguments:    -e     just show information about the elf executable; don't actually run it\n" );
#ifdef RVOS
    printf( "                 -g     (internal) generate rcvtable.txt then exit\n" );
#endif
    printf( "                 -h:X   # of meg for the heap (brk space). 0..1024 are valid. default is 40\n" );
    printf( "                 -i     if -t is set, also enables instruction tracing\n" );
    printf( "                 -m:X   # of meg for mmap space. 0..1024 are valid. default is 40\n" );
    printf( "                 -p     shows performance information at app exit\n" );
    printf( "                 -t     enable debug tracing to %ls\n", LOGFILE_NAME );
    printf( "                 -v     used with -e shows verbose information (e.g. symbols)\n" );
    printf( "  %s\n", build_string() );
    exit( 1 );
} //usage

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
    // Translate open() flags from Linux to Windows
    // Microsoft C uses different constants for flags than linux:
    //             msft (win+dos)     linux
    //             --------------     -----
    // 0           O_RDONLY           O_RDONLY
    // 1           O_WRONLY           O_WRONLY
    // 2           O_RDRW             O_RDWR
    // 0x8         O_APPEND           n/a
    // 0x10        O_RANDOM           _FMARK / O_SHLOCK / _FSHLOCK / O_SYNC
    // 0x20        O_SEQUENTIAL       _FDEFER / O_EXLOCK / _FEXLOCK
    // 0x40        O_TEMPORARY        O_CREAT
    // 0x80        O_NOINHERIT        O_EXCL
    // 0x100       O_CREAT            n/a
    // 0x200       O_TRUNC            O_TRUNC
    // 0x400       O_EXCL             O_APPEND
    // 0x2000      O_OBTAINDIR        O_ASYNC
    // 0x4000      O_TEXT             n/a
    // 0x8000      O_BINARY           n/a
    // 0x10100     n/a                O_FSYNC, O_SYNC

    int f = flags & 3; // copy rd/wr/rdrw

    f |= O_BINARY; // this is assumed on Linux systems

    if ( 0x40 & flags )
        f |= O_CREAT;

    if ( 0x80 & flags )
        f |= O_EXCL;

    if ( 0x200 & flags )
        f |= O_TRUNC;

    if ( 0x400 & flags )
        f |= O_APPEND;

    tracer.Trace( "  flags translated from linux %x to Microsoft %x\n", flags, f );
    return f;
} //windows_translate_flags

uint32_t epoch_days( uint16_t y, uint16_t m, uint16_t d ) // taken from https://blog.reverberate.org/2020/05/12/optimizing-date-algorithms.html
{
    const uint32_t year_base = 4800;    /* Before min year, multiple of 400. */
    const uint32_t m_adj = m - 3;       /* March-based month. */
    const uint32_t carry = m_adj > m ? 1 : 0;
    const uint32_t adjust = carry ? 12 : 0;
    const uint32_t y_adj = y + year_base - carry;
    const uint32_t month_days = ((m_adj + adjust) * 62719 + 769) / 2048;
    const uint32_t leap_days = y_adj / 4 - y_adj / 100 + y_adj / 400;
    return y_adj * 365 + leap_days + month_days + (d - 1) - 2472632;
} //epoch_days

uint64_t SystemTime_to_esecs( SYSTEMTIME & st )
{
    // takes a Windows system time and returns linux epoch seconds
    uint32_t edays = epoch_days( st. wYear, st.wMonth, st.wDay );
    uint32_t secs = ( st.wHour * 3600 ) + ( st.wMinute * 60 ) + st.wSecond;
    return ( edays * 24 * 3600 ) + secs;
} //SystemTime_to_esecs

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
    {
        if ( isatty( descriptor ) )
            pstat->st_mode = S_IFCHR;
        else
        {
            pstat->st_mode = S_IFREG;
            pstat->st_rdev = 4096; // this is st_blksize on linux
        }
    }
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
    else if ( osreleaseDescriptor == descriptor )
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

        if ( ok )
        {
            if ( data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
                pstat->st_mode = S_IFDIR;
            else
                pstat->st_mode = S_IFREG;

            // in spirit: st_mtim = data.ftLastWriteTime;

            SYSTEMTIME st = {0};
            FileTimeToSystemTime( &data.ftLastWriteTime, &st );
            pstat->st_mtim.tv_sec = SystemTime_to_esecs( st );
        }

        pstat->st_rdev = 4096; // this is st_blksize on linux

        if ( !ok && ( descriptor > 0 ) )
        {
            struct _stat statbuf;
            int result = _fstat( descriptor, &statbuf );
            if ( 0 == result )
            {
                pstat->st_mtim.tv_sec = statbuf.st_mtime;
                data.nFileSizeLow = statbuf.st_size;
            }
            else
                return -1;
        }

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

#if !defined(__APPLE__)
#if defined(__ARM_32BIT_STATE) || defined(__ARM_64BIT_STATE) || defined(__riscv)
    int linux_swap_riscv64_arm_dir_open_flags( int flags )
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

        tracer.Trace( "  O_DIRECT %#x, O_DIRECTORY %#x\n", O_DIRECT, O_DIRECTORY );
        tracer.Trace( "  mapped from flags %#x to flags %#x\n", flags, r );
        return r;
    } //linux_swap_riscv64_arm_dir_open_flags
#endif
#endif

struct SysCall
{
    const char * name;
    uint64_t id;
};

static const SysCall syscalls[] =
{
    { "SYS_getcwd", SYS_getcwd },
    { "SYS_fcntl", SYS_fcntl },
    { "SYS_ioctl", SYS_ioctl },
    { "SYS_mkdirat", SYS_mkdirat },
    { "SYS_unlinkat", SYS_unlinkat },
    { "SYS_renameat", SYS_renameat },
    { "SYS_faccessat", SYS_faccessat },
    { "SYS_chdir", SYS_chdir },
    { "SYS_openat", SYS_openat },
    { "SYS_close", SYS_close },
    { "SYS_getdents64", SYS_getdents64 },
    { "SYS_lseek", SYS_lseek },
    { "SYS_read", SYS_read },
    { "SYS_write", SYS_write },
    { "SYS_writev", SYS_writev },
    { "SYS_pselect6", SYS_pselect6 },
    { "SYS_ppoll_time32", SYS_ppoll_time32 },
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
    { "SYS_sched_yield", SYS_sched_yield },
    { "SYS_tgkill", SYS_tgkill },
    { "SYS_signalstack", SYS_signalstack },
    { "SYS_sigaction", SYS_sigaction },
    { "SYS_rt_sigprocmask", SYS_rt_sigprocmask },
    { "SYS_times", SYS_times },
    { "SYS_uname", SYS_uname },
    { "SYS_getrusage", SYS_getrusage },
    { "SYS_prctl", SYS_prctl },
    { "SYS_gettimeofday", SYS_gettimeofday },
    { "SYS_getpid", SYS_getpid },
    { "SYS_getuid", SYS_getuid },
    { "SYS_geteuid", SYS_geteuid },
    { "SYS_getgid", SYS_getgid },
    { "SYS_getegid", SYS_getegid },
    { "SYS_gettid", SYS_gettid },
    { "SYS_sysinfo", SYS_sysinfo },
    { "SYS_brk", SYS_brk },
    { "SYS_munmap", SYS_munmap },
    { "SYS_mremap", SYS_mremap },
    { "SYS_clone", SYS_clone },
    { "SYS_mmap", SYS_mmap },
    { "SYS_mprotect", SYS_mprotect },
    { "SYS_madvise", SYS_madvise },
    { "SYS_riscv_flush_icache", SYS_riscv_flush_icache },
    { "SYS_prlimit64", SYS_prlimit64 },
    { "SYS_renameat2", SYS_renameat2 },
    { "SYS_getrandom", SYS_getrandom },
    { "SYS_rseq", SYS_rseq },
    { "SYS_open", SYS_open }, // only called for older systems
    { "SYS_unlink", SYS_unlink }, // only called for older systems
    { "emulator_sys_rand", emulator_sys_rand },
    { "emulator_sys_print_double", emulator_sys_print_double },
    { "emulator_sys_trace_instructions", emulator_sys_trace_instructions },
    { "emulator_sys_exit", emulator_sys_exit },
    { "emulator_sys_print_text", emulator_sys_print_text },
    { "emulator_sys_get_datetime", emulator_sys_get_datetime },
    { "emulator_sys_print_int64", emulator_sys_print_int64 },
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

void update_result_errno( CPUClass & cpu, int64_t result )
{

    if ( result >= 0 || result <= -4096 ) // syscalls like write() return positive values to indicate success.
    {
        tracer.Trace( "  syscall success, returning %lld = %#llx\n", result, result );
        cpu.regs[ REG_RESULT ] = result;
    }
    else
    {
        tracer.Trace( "  returning negative errno in a0: %d\n", -errno );
        cpu.regs[ REG_RESULT ] = -errno; // it looks like the g++ runtime copies the - of this value to errno
    }
} //update_result_errno

// this is called when the arm64 app has an svc #0 instruction or a RISC-V 64 app has an ecall instruction
// https://thevivekpandey.github.io/posts/2017-09-25-linux-system-calls.html

#pragma warning(disable: 4189) // unreferenced local variable

void emulator_invoke_svc( CPUClass & cpu )
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
        tracer.Trace( "syscall %s %llx = %lld, arg0 %llx, arg1 %llx, arg2 %llx, arg3 %llx, arg4 %llx, arg5 %llx\n", 
                      lookup_syscall( cpu.regs[ REG_SYSCALL ] ), cpu.regs[ REG_SYSCALL ], cpu.regs[ REG_SYSCALL ],
                      cpu.regs[ REG_ARG0 ], cpu.regs[ REG_ARG1 ], cpu.regs[ REG_ARG2 ], cpu.regs[ REG_ARG3 ],
                      cpu.regs[ REG_ARG4 ], cpu.regs[ REG_ARG5 ] );

    switch ( cpu.regs[ REG_SYSCALL ] )
    {
        case emulator_sys_exit: // exit
        case SYS_exit:
        case SYS_exit_group:
        case SYS_tgkill:
        {
            g_terminate = true;
            cpu.end_emulation();
            g_exit_code = (int) cpu.regs[ REG_ARG0 ];
            tracer.Trace( "  emulated app exit code %d\n", g_exit_code );
            update_result_errno( cpu, 0 );
            break;
        }
        case SYS_signalstack:
        {
            update_result_errno( cpu, 0 );
            break;
        }
        case emulator_sys_print_int64:
        {
            printf( "%lld", cpu.regs[ REG_ARG0 ] );
            fflush( stdout );
            update_result_errno( cpu, 0 );
            break;
        }
        case emulator_sys_print_text: // print asciiz in a0
        {
            tracer.Trace( "  syscall command print string '%s'\n", (char *) cpu.getmem( cpu.regs[ REG_ARG0 ] ) );
            printf( "%s", (char *) cpu.getmem( cpu.regs[ REG_ARG0 ] ) );
            fflush( stdout );
            update_result_errno( cpu, 0 );
            break;
        }
        case emulator_sys_get_datetime: // writes local date/time into string pointed to by a0
        {
            char * pdatetime = (char *) cpu.getmem( cpu.regs[ REG_ARG0 ] );
            system_clock::time_point now = system_clock::now();
            uint64_t ms = duration_cast<milliseconds>( now.time_since_epoch() ).count() % 1000;
            time_t time_now = system_clock::to_time_t( now );
            struct tm * plocal = localtime( & time_now );
            snprintf( pdatetime, 80, "%02u:%02u:%02u.%03u", (uint32_t) plocal->tm_hour, (uint32_t) plocal->tm_min, (uint32_t) plocal->tm_sec, (uint32_t) ms );
            tracer.Trace( "  got datetime: '%s', pc: %llx\n", pdatetime, cpu.pc );
            update_result_errno( cpu, 0 );
            break;
        }
        case SYS_getcwd:
        {
            // the syscall version of getcwd never uses malloc to allocate a return value. Just C runtimes do that for POSIX compliance.

            uint64_t original = cpu.regs[ REG_ARG0 ];
            tracer.Trace( "  address in vm space: %llx == %llu\n", original, original );
            char * pin = (char *) cpu.getmem( cpu.regs[ REG_ARG0 ] );
            size_t size = (size_t) cpu.regs[ REG_ARG1 ];
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
                update_result_errno( cpu, original );
            else
                update_result_errno( cpu, errno );
            break;
        }
        case SYS_fcntl:
        {
            int fd = (int) cpu.regs[ REG_ARG0 ];
            int op = (int) cpu.regs[ REG_ARG1 ];

            if ( 1 == op ) // F_GETFD
                cpu.regs[ REG_RESULT ] = 1; // FD_CLOEXEC
            else
                tracer.Trace( "unhandled SYS_fcntl operation %d\n", op );
            break;
        }
        case SYS_clock_nanosleep:
        {
            clockid_t clockid = (clockid_t) cpu.regs[ REG_ARG0 ];
            int flags = (int) cpu.regs[ REG_ARG1 ];
            tracer.Trace( "  nanosleep id %d flags %x\n", clockid, flags );

            const struct timespec_syscall * request = (const struct timespec_syscall *) cpu.getmem( cpu.regs[ REG_ARG2 ] );
            struct timespec_syscall * remain = 0;
            if ( 0 != cpu.regs[ REG_ARG3 ] )
                remain = (struct timespec_syscall *) cpu.getmem( cpu.regs[ REG_ARG3 ] );

            uint64_t ms = request->tv_sec * 1000 + request->tv_nsec / 1000000;
            tracer.Trace( "  nanosleep sec %lld, nsec %lld == %lld ms\n", request->tv_sec, request->tv_nsec, ms );
            sleep_ms( ms );
            update_result_errno( cpu, 0 );
            break;
        }
        case SYS_sched_setaffinity:
        {
            tracer.Trace( "  setaffinity, EPERM %d\n", EPERM );
            update_result_errno( cpu, EPERM );
            break;
        }
        case SYS_sched_getaffinity:
        {
            tracer.Trace( "  getaffinity, EPERM %d\n", EPERM );
            update_result_errno( cpu, EPERM );
            break;
        }
        case SYS_sched_yield:
        {
            // always succeeds on Linux, even though nothing happens here.
            update_result_errno( cpu, 0 );
            break;
        }
        case SYS_newfstat:
        {
            // two arguments: descriptor and pointer to the stat buf
            tracer.Trace( "  syscall command SYS_newfstat\n" );
            int descriptor = (int) cpu.regs[ REG_ARG0 ];
            int result = 0;

#ifdef _WIN32
            struct stat_linux_syscall local_stat = {0};
            result = fill_pstat_windows( descriptor, & local_stat, 0 );
            if ( 0 == result )
            {
                size_t cbStat = sizeof( struct stat_linux_syscall );
                tracer.Trace( "  sizeof stat_linux_syscall: %zd\n", cbStat );
                assert( 128 == cbStat );  // 128 is the size of the stat struct this syscall on RISC-V Linux
                memcpy( cpu.getmem( cpu.regs[ REG_ARG1 ] ), & local_stat, cbStat );
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

                struct stat_linux_syscall * pout = (struct stat_linux_syscall *) cpu.getmem( cpu.regs[ REG_ARG1 ] );
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

            update_result_errno( cpu, result );
            break;
        }
        case SYS_gettimeofday:
        {
            tracer.Trace( "  syscall command SYS_gettimeofday\n" );
            linux_timeval * ptimeval = (linux_timeval *) cpu.getmem( cpu.regs[ REG_ARG0 ] );
            int result = 0;
            if ( 0 != ptimeval )
                result = gettimeofday( ptimeval );

            cpu.regs[ REG_RESULT ] = result;
            break;
        }
        case SYS_lseek:
        {
            tracer.Trace( "  syscall command SYS_lseek\n" );
            int descriptor = (int) cpu.regs[ REG_ARG0 ];
            int offset = (int) cpu.regs[ REG_ARG1 ];
            int origin = (int) cpu.regs[ REG_ARG2 ];

            long result = lseek( descriptor, offset, origin );
            update_result_errno( cpu, result );
            break;
        }
        case SYS_read:
        {
            int descriptor = (int) cpu.regs[ REG_ARG0 ];
            void * buffer = cpu.getmem( cpu.regs[ REG_ARG1 ] );
            unsigned buffer_size = (unsigned) cpu.regs[ REG_ARG2 ];
            tracer.Trace( "  syscall command SYS_read. descriptor %d, buffer %llx, buffer_size %u\n", descriptor, cpu.regs[ REG_ARG1 ], buffer_size );

            if ( 0 == descriptor ) //&& 1 == buffer_size )
            {
#ifdef _WIN32
                int r = g_consoleConfig.linux_getch();
#else
                int r = g_consoleConfig.portable_getch();
#endif
                * (char *) buffer = (char) r;
                cpu.regs[ REG_RESULT ] = 1;
                tracer.Trace( "  getch read character %u == '%c'\n", r, printable( (uint8_t) r ) );
                break;
            }
            else if ( timebaseFrequencyDescriptor == descriptor && buffer_size >= 8 )
            {
                uint64_t freq = 1000000000000; // nanoseconds, but the LPi4a is off by 3 orders of magnitude, so I replicate that bug here
                memcpy( buffer, &freq, 8 );
                update_result_errno( cpu, 8 );
                break;
            }
            else if ( osreleaseDescriptor == descriptor && buffer_size >= 8 )
            {
                memcpy( buffer, "9.69", 5 );
                update_result_errno( cpu, 5 );
                break;
            }

            int result = read( descriptor, buffer, buffer_size );
            if ( result > 0 )
                tracer.TraceBinaryData( (uint8_t *) buffer, (int) get_min( (int) 0x100, result ), 4 );
            update_result_errno( cpu, result );
            break;
        }
        case SYS_write:
        {
            tracer.Trace( "  syscall command SYS_write. fd %lld, buf %llx, count %lld\n", cpu.regs[ REG_ARG0 ], cpu.regs[ REG_ARG1 ], cpu.regs[ REG_ARG2 ] );

            int descriptor = (int) cpu.regs[ REG_ARG0 ];
            uint8_t * p = (uint8_t *) cpu.getmem( cpu.regs[ REG_ARG1 ] );
            uint64_t count = cpu.regs[ REG_ARG2 ];

            if ( 0 == descriptor ) // stdin
            {
                errno = EACCES;
                update_result_errno( cpu, -1 );
            }
            else
            {
                if ( 1 == descriptor || 2 == descriptor ) // stdout / stderr
                    tracer.Trace( "  writing '%.*s'\n", (int) count, p );

                tracer.TraceBinaryData( p, (uint32_t) count, 4 );
                size_t written = write( descriptor, p, (int) count );
                update_result_errno( cpu, written );
            }
            break;
        }
        case SYS_open:
        {
            tracer.Trace( "  syscall command SYS_open\n" );

            // a0: asciiz string of file to open. a1: flags. a2: mode

            const char * pname = (const char *) cpu.getmem( cpu.regs[ REG_ARG0 ] );
            int flags = (int) cpu.regs[ REG_ARG1 ];
            int mode = (int) cpu.regs[ REG_ARG2 ];

            tracer.Trace( "  open flags %x, mode %x, file %s\n", flags, mode, pname );

#ifdef _WIN32
            flags = windows_translate_flags( flags );
#endif

            int descriptor = open( pname, flags, mode );
            tracer.Trace( "  descriptor: %d\n", descriptor );
            update_result_errno( cpu, descriptor );
            break;
        }
        case SYS_close:
        {
            tracer.Trace( "  syscall command SYS_close\n" );
            int descriptor = (int) cpu.regs[ REG_ARG0 ];

            if ( descriptor >=0 && descriptor <= 3 )
            {
                // built-in handle stdin, stdout, stderr -- ignore
                cpu.regs[ REG_RESULT ] = 0;
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
                else if ( timebaseFrequencyDescriptor == descriptor || osreleaseDescriptor == descriptor )
                {
                    update_result_errno( cpu, 0 );
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
                    update_result_errno( cpu, 0 );
                    break;
                }
    #endif
#endif
                result = close( descriptor );
                update_result_errno( cpu, result );
            }
            break;
        }
        case SYS_getdents64:
        {
            int64_t result = 0;
            uint64_t descriptor = cpu.regs[ REG_ARG0 ];
            uint8_t * pentries = (uint8_t *) cpu.getmem( cpu.regs[ REG_ARG1 ] );
            uint64_t count = cpu.regs[ REG_ARG2 ];
            tracer.Trace( "  pentries: %p, count %llu\n", pentries, count );
            struct linux_dirent64_syscall * pcur = (struct linux_dirent64_syscall *) pentries;
            memset( pentries, 0, count );

#ifdef _WIN32
            if ( ( findFirstDescriptor != descriptor ) || ( 0 == g_acFindFirstPattern[ 0 ] ) )
            {
                tracer.Trace( "  getdents on unexpected descriptor or FindFirst (%p) not open\n", g_hFindFirst );
                errno = EBADF;
                update_result_errno( cpu, -1 );
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
                update_result_errno( cpu, -1 );
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
            update_result_errno( cpu, result );
            break;
        }
        case SYS_brk:
        {
            uint64_t original = g_brk_offset;
            uint64_t ask = cpu.regs[ REG_ARG0 ];
            if ( 0 == ask )
                cpu.regs[ REG_RESULT ] = cpu.get_vm_address( g_brk_offset );
            else
            {
                uint64_t ask_offset = ask - g_base_address;
                tracer.Trace( "  ask_offset %llx, g_end_of_data %llx, end_of_stack %llx\n", ask_offset, g_end_of_data, g_bottom_of_stack );

                if ( ask_offset >= g_end_of_data && ask_offset < g_bottom_of_stack )
                {
                    g_brk_offset = cpu.getoffset( ask );
                    if ( g_brk_offset > g_highwater_brk )
                        g_highwater_brk = g_brk_offset;
                }
                else
                {
                    tracer.Trace( "  allocation request was too large, failing it by returning current brk\n" );
                    cpu.regs[ REG_RESULT ] = cpu.get_vm_address( g_brk_offset );
                }
            }

            tracer.Trace( "  SYS_brk. ask %llx, current brk %llx, new brk %llx, result in a0 %llx\n", ask, original, g_brk_offset, cpu.regs[ REG_ARG0 ] );
            break;
        }
        case SYS_munmap:
        {
            uint64_t address = cpu.regs[ REG_ARG0 ];
            uint64_t length = cpu.regs[ REG_ARG1 ];
            length = round_up( length, (uint64_t) 4096 );

            bool ok = g_mmap.free( address, length );
            if ( ok )
                update_result_errno( cpu, 0 );
            else
            {
                errno = EINVAL;
                update_result_errno( cpu, -1 );
            }

            break;
        }
        case SYS_mremap:
        {
            uint64_t address = cpu.regs[ REG_ARG0 ];
            uint64_t old_length = cpu.regs[ REG_ARG1 ];
            uint64_t new_length = cpu.regs[ REG_ARG2 ];
            int flags = (int) cpu.regs[ REG_ARG3 ];

            if ( 0 != ( new_length & 0xfff ) )
            {
                tracer.Trace( "  warning: mremap allocation new length isn't 4k-page aligned\n" );
                new_length = round_up( new_length, (uint64_t) 4096 );
            }

            old_length = round_up( old_length, (uint64_t) 4096 );

            // flags: MREMAP_MAYMOVE = 1, MREMAP_FIXED = 2, MREMAP_DONTUNMAP = 3. Ignore them all

            uint64_t result = g_mmap.resize( address, old_length, new_length, ( 1 == flags ) );
            if ( 0 != result )
                update_result_errno( cpu, result );
            else
            {
                errno = ENOMEM;
                update_result_errno( cpu, -1 );
            }

            break;
        }
        case SYS_clone:
        {
            // can't create a new process or thread with this emulator

            errno = EACCES;
            update_result_errno( cpu, -1 );
            break;
        }
        case emulator_sys_rand: // rand64. returns an unsigned random number in a0
        {
            tracer.Trace( "  syscall command generate random number\n" );
            cpu.regs[ REG_RESULT ] = rand64();
            break;
        }
        case emulator_sys_print_double: // print_double in a0
        {
            tracer.Trace( "  syscall command print double in a0\n" );
            double d;
            memcpy( &d, &cpu.regs[ REG_ARG0 ], sizeof d );
            printf( "%lf", d );
            fflush( stdout );
            update_result_errno( cpu, 0 );
            break;
        }
        case emulator_sys_trace_instructions:
        {
            tracer.Trace( "  syscall command trace_instructions %d\n", cpu.regs[ REG_ARG0 ] );
            cpu.regs[ REG_RESULT ] = cpu.trace_instructions( 0 != cpu.regs[ REG_ARG0 ] );
            break;
        }
        case SYS_mmap:
        {
            // The gnu c runtime is ok with this failing -- it just allocates memory instead probably assuming it's an embedded system.
            // Same for the gnu runtime with Rust.
            // But golang actually needs this to succeed.

            uint64_t addr = cpu.regs[ REG_ARG0 ];
            size_t length = cpu.regs[ REG_ARG1 ];
            int prot = (int) cpu.regs[ REG_ARG2 ];
            int flags = (int) cpu.regs[ REG_ARG3 ];
            int fd = (int) cpu.regs[ REG_ARG4 ];
            size_t offset = (size_t) cpu.regs[ REG_ARG5 ];
            tracer.Trace( "  SYS_mmap. addr %llx, length %zd, protection %#x, flags %#x, fd %d, offset %zd\n", addr, length, prot, flags, fd, offset );

            if ( 0 != ( length & 0xfff ) )
            {
                // golang does this. golang runtime is kinda funny.
                tracer.Trace( "  warning: mmap allocation length isn't 4k-page aligned\n" );
                length = round_up( length, (size_t) 4096 );
            }

            if ( 0 == addr )
            {
                if ( 0 == ( length & 0xfff ) )
                {
                    // 2 == MAP_PRIVATE, 0x20 == MAP_ANONYMOUS, 0x100 = MAP_FIXED

                    if ( ( 0 == ( 0x100 & flags ) ) && ( 0x22 == ( 0x22 & flags ) ) )
                    {
                        uint64_t result = g_mmap.allocate( length );
                        if ( 0 != result )
                        {
                            update_result_errno( cpu, result );
                            break;
                        }
                    }
                    else
                        tracer.Trace( "  error: mmap flags %#x aren't supported\n", flags );
                }
                else
                    tracer.Trace( "  error mmap length isn't 4k-page-aligned\n" );
            }
            else
                tracer.Trace( "  mmap allocation at specific address isn't supported\n" );
    
            tracer.Trace( "  mmap failed\n" );
            errno = ENOMEM;
            update_result_errno( cpu, -1 );
            break;
        }
#if !defined(OLDGCC) // the syscalls below aren't invoked from the old g++ compiler and runtime
        case SYS_openat:
        {
            tracer.Trace( "  syscall command SYS_openat\n" );
            // a0: directory. a1: asciiz string of file to open. a2: flags. a3: mode

            // G++ on RISC-V on a opendir() call will call this with 0x80000 set in flags. Ignored for now,
            // but this should call opendir() on non-Windows and on Windows would take substantial code to
            // implement since opendir() doesn't exist. For now it fails with errno 13.

            int directory = (int) cpu.regs[ REG_ARG0 ];
#ifdef __APPLE__
            if ( -100 == directory )
                directory = -2; // Linux vs. MacOS
#endif
            const char * pname = (const char *) cpu.getmem( cpu.regs[ REG_ARG1 ] );
            int flags = (int) cpu.regs[ REG_ARG2 ];
            int mode = (int) cpu.regs[ REG_ARG3 ];
            int64_t descriptor = 0;

            tracer.Trace( "  open dir %d, flags %x, mode %x, file '%s'\n", directory, flags, mode, pname );

            if ( !strcmp( pname, "/proc/device-tree/cpus/timebase-frequency" ) )
            {
                descriptor = timebaseFrequencyDescriptor;
                update_result_errno( cpu, descriptor );
                break;
            }

            if ( !strcmp( pname, "/proc/sys/kernel/osrelease" ) )
            {
                descriptor = osreleaseDescriptor;
                update_result_errno( cpu, descriptor );
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

// if it's rvos running on Arm or armos runnign on risc-v, swap O_DIRECT and O_DIRECTORY

#ifdef RVOS
    #if defined(__ARM_32BIT_STATE) || defined(__ARM_64BIT_STATE)
            flags = linux_swap_riscv64_arm_dir_open_flags( flags );
    #endif // ARM32 or ARM64
#elif defined( ARMOS )
    #if defined (__riscv)
            flags = linux_swap_riscv64_arm_dir_open_flags( flags );
    #endif // __riscv
#endif
            descriptor = openat( directory, pname, flags, mode );
#endif
            update_result_errno( cpu, descriptor );
            break;
        }
        case SYS_sysinfo:
        {
#if defined(_WIN32) || defined(__APPLE__)
            errno = EACCES;
            update_result_errno( cpu, -1 );
#else
            int result = sysinfo( (struct sysinfo *) cpu.getmem( cpu.regs[ REG_ARG0 ] ) );
            update_result_errno( cpu, result );
#endif
            break;
        }
        case SYS_newfstatat:
        {
            const char * path = (char *) cpu.getmem( cpu.regs[ REG_ARG1 ] );
            tracer.Trace( "  syscall command SYS_newfstatat, id %lld, path '%s', flags %llx\n", cpu.regs[ REG_ARG0 ], path, cpu.regs[ REG_ARG3 ] );
            int descriptor = (int) cpu.regs[ REG_ARG0 ];
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
                memcpy( cpu.getmem( cpu.regs[ REG_ARG2 ] ), & local_stat, cbStat );
                tracer.Trace( "  file size in bytes: %zd, offsetof st_size: %zd\n", local_stat.st_size, offsetof( struct stat_linux_syscall, st_size ) );
            }
            else
                tracer.Trace( "  fill_pstat_windows failed\n" );
#else
            tracer.Trace( "  sizeof struct stat: %zd\n", sizeof( struct stat ) );
            struct stat local_stat = {0};
            struct stat_linux_syscall local_stat_syscall = {0};
            int flags = (int) cpu.regs[ REG_ARG3 ];
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

                struct stat_linux_syscall * pout = (struct stat_linux_syscall *) cpu.getmem( cpu.regs[ REG_ARG2 ] );
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
            update_result_errno( cpu, result );
            break;
        }
        case SYS_chdir:
        {
            const char * path = (const char *) cpu.getmem( cpu.regs[ REG_ARG0 ] );
            tracer.Trace( "  syscall command SYS_chdir path %s\n", path );
#ifdef _WIN32
            int result = _chdir( path );
#else
            int result = chdir( path );
#endif
            update_result_errno( cpu, result );
            break;
        }
        case SYS_mkdirat:
        {
            int directory = (int) cpu.regs[ REG_ARG0 ];
            const char * path = (const char *) cpu.getmem( cpu.regs[ REG_ARG1 ] );
#ifdef _WIN32 // on windows ignore the directory and mode arguments
            tracer.Trace( "  syscall command SYS_mkdirat path %s\n", path );
            int result = _mkdir( path );
#else
#ifdef __APPLE__
            if ( -100 == directory )
                directory = -2;
#endif     
            mode_t mode = (mode_t) cpu.regs[ REG_ARG2 ];
            tracer.Trace( "  syscall command SYS_mkdirat dir %d, path %s, mode %x\n", directory, path, mode );
            int result = mkdirat( directory, path, mode );
#endif
            update_result_errno( cpu, result );
            break;
        }
        case SYS_unlinkat:
        {
            int directory = (int) cpu.regs[ REG_ARG0 ];
            const char * path = (const char *) cpu.getmem( cpu.regs[ REG_ARG1 ] );
            int flags = (int) cpu.regs[ REG_ARG2 ];
            tracer.Trace( "  syscall command SYS_unlinkat dir %d, path %s, flags %x\n", directory, path, flags );
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
            update_result_errno( cpu, result );
            break;
        }
        case SYS_unlink:
        {
            const char * path = (const char *) cpu.getmem( cpu.regs[ REG_ARG0 ] );
            tracer.Trace( "  syscall command SYS_unlink path %s\n", path );
#ifdef _WIN32
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
            int result = unlink( path );
#endif
            update_result_errno( cpu, result );
            break;
        }
        case SYS_uname:
        {
            struct utsname_syscall * pname = (struct utsname_syscall *) cpu.getmem( cpu.regs[ REG_ARG0 ] );
            strcpy( pname->sysname, "syscall" );
            strcpy( pname->nodename, "localhost" );
            strcpy( pname->release, "19.69.420" );
            strcpy( pname->version, "#1" );
            strcpy( pname->machine, "aarch64" );
            strcpy( pname->domainname, "localdomain" );
            update_result_errno( cpu, 0 );
            break;
        }
        case SYS_getrusage:
        {
            int who = (int) cpu.regs[ REG_ARG0 ];
            struct linux_rusage_syscall *prusage = (struct linux_rusage_syscall *) cpu.getmem( cpu.regs[ REG_ARG1 ] );
            memset( prusage, 0, sizeof( struct linux_rusage_syscall ) );

            if ( 0 == who ) // RUSAGE_SELF
            {
#ifdef _WIN32
                FILETIME ftCreation, ftExit, ftKernel, ftUser;
                if ( GetProcessTimes( GetCurrentProcess(), &ftCreation, &ftExit, &ftKernel, &ftUser ) )
                {
                    uint64_t utotal = ( ( (uint64_t) ftUser.dwHighDateTime << 32 ) + ftUser.dwLowDateTime ) / 10; // 100ns to microseconds
                    prusage->ru_utime.tv_sec = utotal / 1000000;
                    prusage->ru_utime.tv_usec = utotal % 1000000;
                    uint64_t stotal = ( ( (uint64_t) ftKernel.dwHighDateTime << 32 ) + ftKernel.dwLowDateTime ) / 10; 
                    prusage->ru_stime.tv_sec = stotal / 1000000;
                    prusage->ru_stime.tv_usec = stotal % 1000000;
                }
                else
                    tracer.Trace( "  unable to GetProcessTimes, error %d\n", GetLastError() );
#else
                struct rusage local_rusage;
                getrusage( who, &local_rusage ); // on 32-bit systems fields are 32 bit
                prusage->ru_utime.tv_sec = local_rusage.ru_utime.tv_sec;
                prusage->ru_utime.tv_usec = local_rusage.ru_utime.tv_usec;
                prusage->ru_stime.tv_sec = local_rusage.ru_stime.tv_sec;
                prusage->ru_stime.tv_usec = local_rusage.ru_stime.tv_usec;
                prusage->ru_maxrss = local_rusage.ru_maxrss;
                prusage->ru_ixrss = local_rusage.ru_ixrss;
                prusage->ru_idrss = local_rusage.ru_idrss;
                prusage->ru_isrss = local_rusage.ru_isrss;
                prusage->ru_minflt = local_rusage.ru_minflt;
                prusage->ru_majflt = local_rusage.ru_majflt;
                prusage->ru_nswap = local_rusage.ru_nswap;
                prusage->ru_inblock = local_rusage.ru_inblock;
                prusage->ru_oublock = local_rusage.ru_oublock;                
                prusage->ru_msgsnd = local_rusage.ru_msgsnd;
                prusage->ru_msgrcv = local_rusage.ru_msgrcv;                                                                                                
                prusage->ru_nsignals = local_rusage.ru_nsignals;
                prusage->ru_nvcsw = local_rusage.ru_nvcsw;
                prusage->ru_nivcsw = local_rusage.ru_nivcsw;
#endif
            }
            else
                tracer.Trace( "  unsupported request for who %u\n", who );

            update_result_errno( cpu, 0 );
        }
        case SYS_futex: 
        {
            if ( !cpu.is_address_valid( cpu.regs[ REG_ARG0 ] ) ) // sometimes it's malformed on arm64. not sure why yet.
            {
                tracer.Trace( "futex pointer in reg 0 is malformed\n" );
                cpu.regs[ REG_RESULT ] = 0;
                break;
            }

            uint32_t * paddr = (uint32_t *) cpu.getmem( cpu.regs[ REG_ARG0 ] );
            int futex_op = (int) cpu.regs[ REG_ARG1 ] & (~128); // strip "private" flags
            uint32_t value = (uint32_t) cpu.regs[ REG_ARG2 ];

            tracer.Trace( "  futex all paddr %p (%d), futex_op %d, val %d\n", paddr, ( 0 == paddr ) ? -666 : *paddr, futex_op, value );

            if ( 0 == futex_op ) // FUTEX_WAIT
            {
                if ( *paddr != value )
                    cpu.regs[ REG_RESULT ] = 11; // EAGAIN
                else
                    cpu.regs[ REG_RESULT ] = 0;
            }    
            else if ( 1 == futex_op ) // FUTEX_WAKE
                cpu.regs[ REG_RESULT ] = 0;
            else    
                cpu.regs[ REG_RESULT ] = (uint64_t) -1; // fail this until/unless there is a real-world use
            break;
        }
        case SYS_writev:
        {
            int descriptor = (int) cpu.regs[ REG_ARG0 ];
            const struct iovec * pvec = (const struct iovec *) cpu.getmem( cpu.regs[ REG_ARG1 ] );
            if ( 1 == descriptor || 2 == descriptor )
                tracer.Trace( "  desc %d: writing '%.*s'\n", descriptor, pvec->iov_len, cpu.getmem( (uint64_t) pvec->iov_base ) );

#ifdef _WIN32
            int64_t result = write( descriptor, cpu.getmem( (uint64_t) pvec->iov_base ), (unsigned) pvec->iov_len );
#else
            struct iovec vec_local;
            vec_local.iov_base = cpu.getmem( (uint64_t) pvec->iov_base );
            vec_local.iov_len = pvec->iov_len;
            tracer.Trace( "  write length: %u to descriptor %d at addr %p\n", pvec->iov_len, descriptor, vec_local.iov_base );
            int64_t result = writev( descriptor, &vec_local, cpu.regs[ REG_ARG2 ] );
#endif
            update_result_errno( cpu, result );
            break;
        }
        case SYS_clock_gettime:
        {
            clockid_t cid = (clockid_t) cpu.regs[ REG_ARG0 ];
            struct timespec_syscall * ptimespec = (struct timespec_syscall *) cpu.getmem( cpu.regs[ REG_ARG1 ] );
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
            update_result_errno( cpu, result );
            break;
        }
        case SYS_fdatasync:
        {
            int descriptor = (int) cpu.regs[ REG_ARG0 ];

#ifdef _WIN32
            int result = _commit( descriptor );
#elif defined( __APPLE__ )
            int result = fsync( descriptor ); // fdatasync isn't available on MacOS
#else
            int result = fdatasync( descriptor );
#endif
            update_result_errno( cpu, result );
            break;
        }
        case SYS_sigaction:
        {
            //errno = EACCES;
            //update_result_errno( cpu, -1 );
            update_result_errno( cpu, 0 );
            break;
        }
        case SYS_times:
        {
            // this function is long obsolete, but older apps call it and it's still supported on recent Linux builds

            struct linux_tms_syscall * ptms = ( 0 != cpu.regs[ REG_ARG0 ] ) ? (struct linux_tms_syscall *) cpu.getmem( cpu.regs[ REG_ARG0 ] ) : 0;
            if ( 0 != ptms ) // 0 is legal if callers just want the return value
            {
                memset( ptms, 0, sizeof ( struct linux_tms_syscall ) );
#ifdef _WIN32
                FILETIME ftCreation, ftExit, ftKernel, ftUser;
                if ( GetProcessTimes( GetCurrentProcess(), &ftCreation, &ftExit, &ftKernel, &ftUser ) )
                {
                    ptms->tms_utime = ( ( (uint64_t) ftUser.dwHighDateTime << 32) + ftUser.dwLowDateTime ) / 100000; // 100ns to hundredths of a second
                    ptms->tms_stime = ( ( (uint64_t) ftKernel.dwHighDateTime << 32 ) + ftKernel.dwLowDateTime ) / 100000; 
                }
                else
                    tracer.Trace( "  unable to GetProcessTimes, error %d\n", GetLastError() );
#else
                struct tms local_tms; // on 32-bit systems the members are 32 bit.
                times( &local_tms );
                ptms->tms_utime = local_tms.tms_utime;
                ptms->tms_stime = local_tms.tms_stime;
                ptms->tms_cutime = local_tms.tms_cutime;
                ptms->tms_cstime = local_tms.tms_cstime;
#endif

            }

            struct linux_timeval tv;
            gettimeofday( &tv );

            // ticks is generally in hundredths of a second per sysconf( _SC_CLK_TCK )
            uint64_t sc_clk_tck = 100ull;
#ifndef _WIN32
            sc_clk_tck = sysconf( _SC_CLK_TCK );
#endif
            uint64_t ticks = ( (uint64_t) tv.tv_sec * sc_clk_tck ) + ( ( (uint64_t) tv.tv_usec * sc_clk_tck ) / 1000000ull );
            update_result_errno( cpu, ticks );
            break;
        }
        case SYS_rt_sigprocmask:
        {
            errno = 0;
            update_result_errno( cpu, 0 );
            break;
        }
        case SYS_prctl:
        {
            update_result_errno( cpu, 0 );
            break;
        }
        case SYS_getpid:
        {
            cpu.regs[ REG_RESULT ] = getpid();
            break;
        }
        case SYS_gettid:
        {
            cpu.regs[ REG_RESULT ] = 1;
            break;
        }
        case SYS_renameat:
        case SYS_renameat2:
        {
            int olddirfd = (int) cpu.regs[ REG_ARG0 ];
            const char * oldpath = (const char *) cpu.getmem( cpu.regs[ REG_ARG1 ] );
            int newdirfd = (int) cpu.regs[ REG_ARG2 ];
            const char * newpath = (const char *) cpu.getmem( cpu.regs[ REG_ARG3 ] );
            unsigned int flags = ( SYS_renameat2 == cpu.regs[ 8 ] ) ? (unsigned int) cpu.regs[ REG_ARG4 ] : 0;
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
            update_result_errno( cpu, result );
            break;
        }
        case SYS_getrandom:
        {
            void * buf = cpu.getmem( cpu.regs[ REG_ARG0 ] );
            size_t buflen = cpu.regs[ REG_ARG1 ];
            unsigned int flags = (unsigned int) cpu.regs[ REG_ARG2 ];
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
            update_result_errno( cpu, result );
            break;
        }
        case SYS_rseq :
        {
            errno = EPERM;
            update_result_errno( cpu, -1 );
            break;
        }
        case SYS_riscv_flush_icache :
        {
            assert( false ); // no arm64 equivalent
            cpu.regs[ REG_RESULT ] = 0;
            break;
        }
        case SYS_pselect6:
        {
            int nfds = (int) cpu.regs[ REG_ARG0 ];
            void * readfds = (void *) cpu.regs[ REG_ARG1 ];

            if ( 1 == nfds && 0 != readfds )
            {
                // check to see if stdin has a keystroke available

                cpu.regs[ REG_RESULT ] = g_consoleConfig.portable_kbhit();
                tracer.Trace( "  pselect6 keystroke available on stdin: %llx\n", cpu.regs[ REG_ARG0 ] );
            }
            else
            {
                // lie and say no I/O is ready

                cpu.regs[ REG_RESULT ] = 0;
            }

            break;
        }
        case SYS_ppoll_time32:
        {
            struct pollfd_syscall * pfds = (struct pollfd_syscall *) cpu.getmem( cpu.regs[ REG_ARG0 ] );
            int nfds = (int) cpu.regs[ REG_ARG1 ];
            struct timespec_syscall * pts = (struct timespec_syscall *) cpu.getmem( cpu.regs[ REG_ARG2 ] );
            int * psigmask = (int *) ( ( 0 == cpu.regs[ REG_ARG3 ] ) ? 0 : cpu.getmem( cpu.regs[ REG_ARG3 ] ) );

            tracer.Trace( "  count of file descriptors: %d\n", nfds );
            for ( int i = 0; i < nfds; i++ )
                tracer.Trace( "    fd %d: %d\n", i, pfds[ i ].fd );

            // lie and say no I/O is ready
            cpu.regs[ REG_RESULT ] = 0;
            break;
        }
        case SYS_readlinkat:
        {
            int dirfd = (int) cpu.regs[ REG_ARG0 ];
            const char * pathname = (const char *) cpu.getmem( cpu.regs[ REG_ARG1 ] );
            char * buf = (char *) cpu.getmem( cpu.regs[ REG_ARG2 ] );
            size_t bufsiz = (size_t) cpu.regs[ REG_ARG3 ];
            tracer.Trace( "  readlinkat pathname %p == '%s', buf %p, bufsiz %zd, dirfd %d\n", pathname, pathname, buf, bufsiz, dirfd );
            int64_t result = -1;

#ifdef _WIN32
            errno = EINVAL; // no symbolic links on Windows as far as this emulator is concerned
            result = -1;
#else
            result = readlinkat( dirfd, pathname, buf, bufsiz );
            tracer.Trace( "  result of readlinkat(): %d\n", result );
#endif

            update_result_errno( cpu, result );
            break;
        }
        case SYS_ioctl:
        {
            int fd = (int) cpu.regs[ REG_ARG0 ];
            unsigned long request = (unsigned long) cpu.regs[ REG_ARG1 ];
            tracer.Trace( "  ioctl fd %d, request %lx\n", fd, request );
            struct local_kernel_termios * pt = (struct local_kernel_termios *) cpu.getmem( cpu.regs[ REG_ARG2 ] );

            if ( 0 == fd || 1 == fd || 2 == fd ) // stdin, stdout, stderr
            {
#ifdef _WIN32
                if ( 0x5401 == request ) // TCGETS
                {
                    if ( isatty( fd ) )
                    {
                        // populate with arbitrary but reasonable values

                        memset( pt, 0, sizeof( *pt ) );
                        pt->c_iflag = 0;
                        pt->c_oflag = 5;
                        pt->c_cflag = 0xbf;
                        pt->c_lflag = 0xa30;
                        update_result_errno( cpu, 0 );
                    }
                    else
                        update_result_errno( cpu, -1 );

                    break;
                }
                else if ( 0x5402 == request )
                {
                    // kbhit() works without all this fuss on Windows
                }
#else
                // likely a TCGETS or TCSETS on stdin to check or enable non-blocking reads for a keystroke

                if ( 0x5401 == request ) // TCGETS
                {
                    struct termios val;
                    int result = tcgetattr( fd, &val );
                    if ( -1 == result )
                    {
                        update_result_errno( cpu, -1 );
                        break;
                    }
                    tracer.Trace( "  result %d, iflag %#x, oflag %#x, cflag %#x, lflag %#x\n", result, val.c_iflag, val.c_oflag, val.c_cflag, val.c_lflag );
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
#endif // _WIN32
            }
            else if ( 1 == fd ) // stdout
            {
                if ( 0x5401 == request ) // TCGETS
                {
#ifdef _WIN32
                    if ( isatty( fd ) )
                        update_result_errno( cpu, 0 );
                    else
                        update_result_errno( cpu, -1 );

                    break;
#else
                    struct termios val;
                    int result = tcgetattr( fd, &val );
                    tracer.Trace( "  result: %d, iflag %#x, oflag %#x, cflag %#x, lflag %#x\n", result, val.c_iflag, val.c_oflag, val.c_cflag, val.c_lflag );
                    if ( -1 == result )
                    {
                        update_result_errno( cpu, -1 );
                        break;
                    }
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
                    tracer.Trace( "  ioctl queried termios on stdout, sizeof local_kernel_termios %zd, sizeof val %zd\n", 
                                sizeof( struct local_kernel_termios ), sizeof( val ) );
#endif // _WIN32
                }
                
            }

            cpu.regs[ REG_RESULT ] = 0;
            break;
        }
        case SYS_set_tid_address:
        {
            cpu.regs[ REG_RESULT ] = 1;
            break;
        }
        case SYS_madvise:
        {
            cpu.regs[ REG_RESULT ] = 0; // report success
            break;
        }
        case SYS_set_robust_list:
        case SYS_prlimit64:
        case SYS_mprotect:
            // ignore for now
            break;

#endif // !defined(OLDGCC)
        case SYS_faccessat:
        {
            const char * pathname = (const char *) cpu.getmem( cpu.regs[ REG_ARG1 ] );
            tracer.Trace( "  faccessat failing for path %s\n", pathname );

            errno = 2; // not found
            update_result_errno( cpu, -1 );
            break;
        }
        case SYS_getuid:
        case SYS_geteuid:
        case SYS_getgid:
        case SYS_getegid:
        {
            update_result_errno( cpu, 0x5549 ); // IU
            break;
        }
        default:
        {
            printf( "error; ecall invoked with unknown command %llu = %llx, a0 %#llx, a1 %#llx, a2 %#llx\n",
                    cpu.regs[ 8 ], cpu.regs[ 8 ], cpu.regs[ REG_ARG0 ], cpu.regs[ REG_ARG1 ], cpu.regs[ REG_ARG2 ] );
            tracer.Trace( "error; ecall invoked with unknown command %llu = %llx, a0 %#llx, a1 %#llx, a2 %#llx\n",
                          cpu.regs[ 8 ], cpu.regs[ 8 ], cpu.regs[ REG_ARG0 ], cpu.regs[ REG_ARG1 ], cpu.regs[ REG_ARG2 ] );
            fflush( stdout );
            //cpu.regs[ REG_RESULT ] = -1;
        }
    }
} //emulator_invoke_svc

#ifdef RVOS
static const char * riscv_register_names[ 32 ] =
{
    "zero", "ra", "sp",  "gp",  "tp", "t0", "t1", "t2",
    "s0",   "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
    "a6",   "a7", "s2",  "s3",  "s4", "s5", "s6", "s7",
    "s8",   "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};
#endif

void emulator_hard_termination( CPUClass & cpu, const char *pcerr, uint64_t error_value )
{
    g_consoleConfig.RestoreConsole( false );

    tracer.Trace( "% (%s) fatal error: %s %0llx\n", APP_NAME, target_platform(), pcerr, error_value );
    printf( "%s (%s) fatal error: %s %0llx\n", APP_NAME, target_platform(), pcerr, error_value );

    uint64_t offset = 0;
    const char * psymbol = emulator_symbol_lookup( cpu.pc, offset );
    tracer.Trace( "pc: %llx %s + %llx\n", cpu.pc, psymbol, offset );
    printf( "pc: %llx %s + %llx\n", cpu.pc, psymbol, offset );

    tracer.Trace( "address space %llx to %llx\n", g_base_address, g_base_address + memory.size() );
    printf( "address space %llx to %llx\n", g_base_address, g_base_address + memory.size() );

    tracer.Trace( "  " );
    printf( "  " );
    for ( size_t i = 0; i < 32; i++ )
    {
#ifdef ARMOS
        tracer.Trace( "%02zu: %16llx, ", i, cpu.regs[ i ] );
        printf( "%02zu: %16llx, ", i, cpu.regs[ i ] );
#elif defined( RVOS )
        tracer.Trace( "%4s: %16llx, ", riscv_register_names[ i ], cpu.regs[ i ] );
        printf( "%4s: %16llx, ", riscv_register_names[ i ], cpu.regs[ i ] );
#endif

        if ( 3 == ( i & 3 ) )
        {
            tracer.Trace( "\n" );
            printf( "\n" );
            if ( 31 != i )
            {
                tracer.Trace( "  " );
                printf( "  " );
            }
        }
    }

#ifdef ARMOS
    cpu.trace_vregs();
#endif

    tracer.Trace( "%s\n", build_string() );
    printf( "%s\n", build_string() );

    tracer.Flush();
    fflush( stdout );
    exit( 1 );
} //emulator_hard_termination

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

const char * emulator_symbol_lookup( uint64_t address, uint64_t & offset )
{
    if ( address < g_base_address || address > ( g_base_address + memory.size() ) )
        return "";

    ElfSymbol64 key = {0};
    key.value = address;

    ElfSymbol64 * psym = (ElfSymbol64 *) bsearch( &key, g_symbols.data(), g_symbols.size(), sizeof( key ), symbol_find_compare );

    if ( 0 != psym )
    {
        offset = address - psym->value;
        return & g_string_table[ psym->name ];
    }

    offset = 0;
    return "";
} //emulator_symbol_lookup

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

void remove_spaces( char * p )
{
    char * o;
    for ( o = p; *p; p++ )
    {
        if ( ' ' != *p )
            *o++ = *p;
    }
    *o = 0;
} //remove_spaces

const char * image_type( uint16_t e_type )
{
    if ( 0 == e_type )
        return "et none";
    if ( 1 == e_type )
        return "et relocatable file";
    if ( 2 == e_type )
        return "et executable";
    if ( 3 == e_type )
        return "et dynamic linked shared object";
    if ( 4 == e_type )
        return "et core file";
    return "et unknown";
} //image_type

bool load_image( const char * pimage, const char * app_args )
{
    tracer.Trace( "loading image %s\n", pimage );

    FILE * fp = fopen( pimage, "rb" );
    if ( !fp )
    {
        printf( "can't open elf image file: %s\n", pimage );
        usage();
    }

    CFile file( fp );
    ElfHeader64 ehead = {0};
    size_t read = fread( &ehead, sizeof ehead, 1, fp );
    if ( 1 != read )
        usage( "elf image file is invalid" );

    if ( 0x464c457f != ehead.magic )
        usage( "elf image file's magic header is invalid" );

    if ( 2 != ehead.type )
    {
        printf( "e_type is %d == %s\n", ehead.type, image_type( ehead.type ) );
        usage( "elf image isn't an executable file (2)" );
    }

    if ( ELF_MACHINE_ISA != ehead.machine )
        usage( "elf image machine ISA doesn't match this emulator" );

    if ( 0 == ehead.entry_point )
        usage( "elf entry point is 0, which is invalid" );

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
            printf( "dynamic linking is not supported by this emulator. link your app with -static\n" );
            exit( 1 );
        }

        uint64_t just_past = head.physical_address + head.memory_size;
        if ( just_past > memory_size )
            memory_size = just_past;

        if ( ( 0 != head.physical_address ) && ( ( 0 == g_base_address ) || g_base_address > head.physical_address ) )
            g_base_address = head.physical_address;
    }

    if ( 0 == g_base_address )
        usage( "base address of elf image is invalid; physical address required" );

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

    // remove symbols that don't look like they have a valid addresses (rust binaries have tens of thousands of these)

    size_t to_erase = 0;
    for ( size_t se = 0; se < g_symbols.size(); se++ )
    {
        if ( g_symbols[ se ].value < g_base_address )
            to_erase++;
        else
            break;
    }

    if ( to_erase > 0 )
        g_symbols.erase( g_symbols.begin(), g_symbols.begin() + to_erase );

    // set the size of each symbol if it's not already set

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

    tracer.Trace( "elf image has %zu usable symbols:\n", g_symbols.size() );
    tracer.Trace( "             address              size  name\n" );

    for ( size_t se = 0; se < g_symbols.size(); se++ )
        tracer.Trace( "    %16llx  %16llx  %s\n", g_symbols[ se ].value, g_symbols[ se ].size, & g_string_table[ g_symbols[ se ].name ] );

    // memory map from high to low addresses:
    //     <end of allocated memory>
    //     (memory for mmap fulfillment)
    //     g_mmap_offset
    //     (wasted space so g_mmap_offset is 4k-aligned)
    //     Linux start data on the stack (see details below)
    //     g_top_of_stack
    //     g_bottom_of_stack
    //     (unallocated space between brk and the bottom of the stack)
    //     g_brk_offset with uninitialized RAM (just after arg_data_offset initially)
    //     g_end_of_data
    //     arg_data_offset
    //     uninitalized data (size read from the .elf file)
    //     initialized data (size & data read from the .elf file)
    //     code (read from the .elf file)
    //     g_base_address (offset read from the .elf file).

    // stacks by convention on arm64 and risc-v are 16-byte aligned. make sure to start aligned

    if ( memory_size & 0xf )
    {
        memory_size += 16;
        memory_size &= ~0xf;
    }

    uint64_t arg_data_offset = memory_size;
    memory_size += g_args_commit;
    g_end_of_data = memory_size;
    g_brk_offset = memory_size;
    g_highwater_brk = memory_size;
    memory_size += g_brk_commit;

    g_bottom_of_stack = memory_size;
    memory_size += g_stack_commit;

    uint64_t top_of_aux = memory_size;
    memory_size = round_up( memory_size, (uint64_t) 4096 ); // mmap should hand out 4k-aligned pages
    g_mmap_offset = memory_size;
    memory_size += g_mmap_commit;

    memory.resize( memory_size );
    memset( memory.data(), 0, memory_size );

    g_mmap.initialize( g_base_address + g_mmap_offset, g_mmap_commit, memory.data() - g_base_address );

    // load the program into RAM

    uint64_t first_uninitialized_data = 0;

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

            first_uninitialized_data = get_max( head.physical_address + head.file_size, first_uninitialized_data );

            tracer.Trace( "  read type %s: %llx bytes into physical address %llx - %llx then uninitialized to %llx \n", head.show_type(), head.file_size,
                          head.physical_address, head.physical_address + head.file_size - 1, head.physical_address + head.memory_size - 1 );
            tracer.TraceBinaryData( memory.data() + head.physical_address - g_base_address, get_min( (uint32_t) head.file_size, (uint32_t) 128 ), 4 );
        }
    }

    // write the command-line arguments into the vm memory in a place where _start can find them.
    // there's an array of pointers to the args followed by the arg strings at offset arg_data_offset.

    uint64_t * parg_data = (uint64_t *) ( memory.data() + arg_data_offset );
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
        parg_data[ app_argc ] = (uint64_t) ( offset + arg_data_offset + g_base_address + max_args * sizeof( uint64_t ) );
        tracer.Trace( "  argument %d is '%s', at vm address %llx\n", app_argc, pargs, parg_data[ app_argc ] );

        app_argc++;
        pargs += strlen( pargs );
    
        if ( space )
            pargs++;
    }

    uint64_t env_offset = args_len + 1;
    char * penv_data = (char *) ( buffer_args + env_offset );
    strcpy( penv_data, "OS=" );
    strcat( penv_data, APP_NAME );
    uint64_t env_os_address = ( penv_data - (char *) memory.data() ) + g_base_address;
    uint64_t env_count = 1;
    uint64_t env_tz_address = 0;

     // The local time zone is found by libc on Linux/MacOS, but not on Windows.
     // Workaround: set the TZ environment variable.

#ifdef _WIN32
    DYNAMIC_TIME_ZONE_INFORMATION tzi = {0};
    DWORD dw = GetDynamicTimeZoneInformation( &tzi );
    if ( TIME_ZONE_ID_INVALID != dw )
    {
        char acName[ 32 ] = {0};
        if ( TIME_ZONE_ID_STANDARD == dw )
            wcstombs( acName, tzi.StandardName, sizeof( acName ) );
        else if ( TIME_ZONE_ID_DAYLIGHT == dw )
            wcstombs( acName, tzi.DaylightName, sizeof( acName ) );
        else if ( TIME_ZONE_ID_UNKNOWN == dw )
            strcpy( acName, "local" );

        if ( 0 != acName[ 0 ] )
        {
            char * ptz_data = penv_data + 1 + strlen( penv_data );
            env_tz_address = ( ptz_data - (char *) memory.data() ) + g_base_address;
            strcpy( ptz_data, "TZ=" );

            // libc doesn't like spaces in spite of the doc saying it's OK:
            // https://ftp.gnu.org/old-gnu/Manuals/glibc-2.2.3/html_node/libc_431.html

            remove_spaces( acName );
            strcat( (char *) ptz_data, acName );

            if ( tzi.Bias >= 0 )
                strcat( (char *) ptz_data, "+" );
            itoa( tzi.Bias / 60, (char *) ( ptz_data + strlen( ptz_data ) ), 10 );
            int minutes = abs( tzi.Bias % 60 );
            if ( 0 != minutes )
            {
                strcat( ptz_data, ":" );
                itoa( minutes, (char *) ( ptz_data + strlen( ptz_data ) ), 10 );
            }
            tracer.Trace( "ptz_data: '%s'\n", ptz_data );
            env_count++;
        }
    }
#endif

    tracer.Trace( "args_len %d, penv_data %p\n", args_len, penv_data );
    tracer.TraceBinaryData( (uint8_t *) ( memory.data() + arg_data_offset ), g_args_commit + 0x20, 4 ); // +20 to inspect for bugs

    // put the Linux startup info at the top of the stack. this consists of (from high to low):
    //   two 8-byte random numbers used for stack and pointer guards
    //   optional filler to make sure alignment of all startup info is 16 bytes
    //   AT_NULL aux record
    //   AT_RANDOM aux record -- point to the 2 random 8-byte numbers above
    //   AT_PAGESZ aux record -- golang runtime fails if it can't find the page size here
    //   0 environment termination
    //   0..n environment string pointers
    //   0 argv termination
    //   1..n argv string pointers
    //   argc  <<<==== sp should point here when the entrypoint (likely _start) is invoked

    uint64_t * pstack = (uint64_t *) ( memory.data() + top_of_aux );

    pstack--;
    *pstack = rand64();
    pstack--;
    *pstack = rand64();
    uint64_t prandom = g_base_address + memory_size - 16;

    // ensure that after all of this the stack is 16-byte aligned

    if ( 0 == ( 1 & ( app_argc + env_count ) ) )
        pstack--;

    pstack -= 2; // the AT_NULL record will be here since memory is initialized to 0

    pstack -= 16; // for 8 aux records
    AuxProcessStart * paux = (AuxProcessStart *) pstack;    
    paux[0].a_type = 25; // AT_RANDOM
    paux[0].a_un.a_val = prandom;
    paux[1].a_type = 6; // AT_PAGESZ
    paux[1].a_un.a_val = 4096;
    paux[2].a_type = 16; // AT_HWCAP
    paux[2].a_un.a_val = 0xa01; // ARM64 bits for fp(0), atomics(8), cpuid(11)
    paux[3].a_type = 26; // AT_HWCAP2
    paux[3].a_un.a_val = 0;
    paux[4].a_type = 11; // AT_UID
    paux[4].a_un.a_val = 0x595a5449; // "ITZY" they are each my bias.
    paux[5].a_type = 22; // AT_EUID;
    paux[5].a_un.a_val = 0x595a5449;
    paux[6].a_type = 13; // AT_GID
    paux[6].a_un.a_val = 0x595a5449;
    paux[7].a_type = 14; // AT_EGID
    paux[7].a_un.a_val = 0x595a5449;

    pstack--; // end of environment data is 0
    pstack--; // move to where the OS environment variable is set OS=RVOS or OS=ARMOS
    *pstack = env_os_address; // (uint64_t) ( env_offset + arg_data_offset + g_base_address + max_args * sizeof( uint64_t ) );
    tracer.Trace( "the OS environment argument is at VM address %llx\n", *pstack );

    if ( 0 != env_tz_address )
    {
        pstack--; // move to where the TZ environment variable is set TZ=xxx
        *pstack = env_tz_address;
        tracer.Trace( "the TZ environment argument is at VM address %llx\n", *pstack );
    }

    pstack--; // the last argv is 0 to indicate the end

    for ( int iarg = (int) app_argc - 1; iarg >= 0; iarg-- )
    {
        pstack--;
        *pstack = parg_data[ iarg ];
    }

    pstack--;
    *pstack = app_argc;

    g_top_of_stack = (uint64_t) ( ( (uint8_t *) pstack - memory.data() ) + g_base_address );
    uint64_t aux_data_size = top_of_aux - (uint64_t) ( (uint8_t *) pstack - memory.data() );
    tracer.Trace( "stack at start (beginning with argc) -- %llu bytes at address %p:\n", aux_data_size, pstack );
    tracer.TraceBinaryData( (uint8_t *) pstack, (uint32_t) aux_data_size, 2 );

    tracer.Trace( "memory map from highest to lowest addresses:\n" );
    tracer.Trace( "  first byte beyond allocated memory:                 %llx\n", g_base_address + memory_size );
    tracer.Trace( "  <mmap arena>                                        (%lld = %llx bytes)\n", g_mmap_commit, g_mmap_commit );
    tracer.Trace( "  mmap start adddress:                                %llx\n", g_base_address + g_mmap_offset );
    tracer.Trace( "  <align to 4k-page for mmap allocations>\n" );
    tracer.Trace( "  start of aux data:                                  %llx\n", g_top_of_stack + aux_data_size );
    tracer.Trace( "  <random, alignment, aux recs, env, argv>            (%lld == %llx bytes)\n", aux_data_size, aux_data_size );
    tracer.Trace( "  initial stack pointer g_top_of_stack:               %llx\n", g_top_of_stack );
    uint64_t stack_bytes = g_stack_commit - aux_data_size;
    tracer.Trace( "  <stack>                                             (%lld == %llx bytes)\n", stack_bytes, stack_bytes );
    tracer.Trace( "  last byte stack can use (g_bottom_of_stack):        %llx\n", g_base_address + g_bottom_of_stack );
    tracer.Trace( "  <unallocated space between brk and the stack>       (%lld == %llx bytes)\n", g_brk_commit, g_brk_commit );
    tracer.Trace( "  end_of_data / current brk:                          %llx\n", g_base_address + g_end_of_data );
    uint64_t argv_bytes = g_end_of_data - arg_data_offset;
    tracer.Trace( "  <argv data, pointed to by argv array above>         (%lld == %llx bytes)\n", argv_bytes, argv_bytes );
    tracer.Trace( "  start of argv data:                                 %llx\n", g_base_address + arg_data_offset );
    uint64_t uninitialized_bytes = g_base_address + arg_data_offset - first_uninitialized_data;
    tracer.Trace( "  <uninitialized data per the .elf file>              (%lld == %llx bytes)\n", uninitialized_bytes, uninitialized_bytes );
    tracer.Trace( "  first byte of uninitialized data:                   %llx\n", first_uninitialized_data );
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

void elf_info( const char * pimage, bool verbose )
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

    if ( ELF_MACHINE_ISA != ehead.machine )
        printf( "image machine ISA isn't a match for %s; continuing anyway. machine type is %x\n", APP_NAME, ehead.machine );

    if ( 2 != ehead.bit_width )
    {
        printf( "image isn't 64-bit (2), it's %u\n", ehead.bit_width );
        return;
    }

    printf( "header fields:\n" );
    printf( "  bit_width: %u\n", ehead.bit_width );
    printf( "  type: %u\n", ehead.type );
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

            if ( verbose )
            {
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
        else if ( 7 == head.type && 0 != head.size )
        {
            vector<uint8_t> notes( head.size );
            fseek( fp, (long) head.offset, SEEK_SET );
            read = fread( notes.data(), 1, head.size, fp );
            if ( 0 == read )
                usage( "can't read notes\n" );
            tracer.PrintBinaryData( notes.data(), (uint32_t) head.size, 4 );
        }
    }

    if ( 0 == g_base_address )
        printf( "base address of elf image is zero; physical address required for the emulator\n" );

    printf( "flags: %#08x\n", ehead.flags );
    printf( "  contains 2-byte compressed RVC instructions: %s\n", g_compressed_rvc ? "yes" : "no" );
    printf( "  contains 4-byte float instructions: %s\n", ( ehead.flags & 2 ) ? "yes" : "no" );
    printf( "  contains 8-byte double instructions: %s\n", ( ehead.flags & 4 ) ? "yes" : "no" );
    printf( "  RV TSO memory consistency: %s\n", ( ehead.flags & 0x10 ) ? "yes" : "no" );
    printf( "  contains non-standard extensions: %s\n", ( ehead.flags & 0xff000000 ) ? "yes" : "no" );
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

int main( int argc, char * argv[] )
{
    try
    {
        bool trace = false;
        char * pcApp = 0;
        bool showPerformance = false;
        bool traceInstructions = false;
        bool elfInfo = false;
        bool verboseElfInfo = false;
        bool generateRVCTable = false;
        static char acAppArgs[1024] = {0};
        static char acApp[1024] = {0};
    
        setlocale( LC_CTYPE, "en_US.UTF-8" );            // these are needed for printf of utf-8 to work
        setlocale( LC_COLLATE, "en_US.UTF-8" );

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
#ifdef RVOS
                else if ( 'g' == ca )
                    generateRVCTable = true;
#endif
                else if ( 'h' == ca )
                {
                    if ( ':' != parg[2] )
                        usage( "the -h argument requires a value" );
    
                    uint64_t heap = strtoull( parg + 3 , 0, 10 );
                    if ( heap > 1024 ) // limit to a gig
                        usage( "invalid heap size specified" );
    
                    g_brk_commit = heap * 1024 * 1024;
                }
                else if ( 'm' == ca )
                {
                    if ( ':' != parg[2] )
                        usage( "the -m argument requires a value" );
    
                    uint64_t mmap_space = strtoull( parg + 3 , 0, 10 );
                    if ( mmap_space > 1024 ) // limit to a gig
                        usage( "invalid mmap size specified" );
    
                    g_mmap_commit = mmap_space * 1024 * 1024;
                }
                else if ( 'e' == ca )
                    elfInfo = true;
                else if ( 'p' == ca )
                    showPerformance = true;
                else if ( 'v' == ca )
                    verboseElfInfo = true;
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
    
        tracer.Enable( trace, LOGFILE_NAME, true );
        tracer.SetQuiet( true );
    
        g_consoleConfig.EstablishConsoleOutput( 0, 0 );

#ifdef RVOS
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
#endif
    
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
            elf_info( acApp, verboseElfInfo );
            g_consoleConfig.RestoreConsole( false );
            return 0;
        }
    
        bool ok = load_image( acApp, acAppArgs );
        if ( ok )
        {
            unique_ptr<CPUClass> cpu( new CPUClass( memory, g_base_address, g_execution_address, g_stack_commit, g_top_of_stack ) );
            cpu->trace_instructions( traceInstructions );
            uint64_t cycles = 0;
    
            high_resolution_clock::time_point tStart = high_resolution_clock::now();
    
            #ifdef _WIN32
                g_tAppStart = tStart;
            #endif
    
            do
            {
                cycles += cpu->run( 1000000 );
            } while( !g_terminate );
    
            char ac[ 100 ];
            if ( showPerformance )
            {
                high_resolution_clock::time_point tDone = high_resolution_clock::now();
                int64_t totalTime = duration_cast<std::chrono::milliseconds>( tDone - tStart ).count();
    
                printf( "elapsed milliseconds:  %15s\n", CDJLTrace::RenderNumberWithCommas( totalTime, ac ) );
                printf( "cycles:                %15s\n", CDJLTrace::RenderNumberWithCommas( cycles, ac ) );
                if ( 0 != totalTime )
                    printf( "effective clock rate:  %15s\n", CDJLTrace::RenderNumberWithCommas( cycles / totalTime, ac ) );
                printf( "app exit code:         %15d\n", g_exit_code );
            }
    
            tracer.Trace( "highwater brk heap:  %15s\n", CDJLTrace::RenderNumberWithCommas( g_highwater_brk - g_end_of_data, ac ) );
            g_mmap.trace_allocations();
            tracer.Trace( "highwater mmap heap: %15s\n", CDJLTrace::RenderNumberWithCommas( g_mmap.peak_usage(), ac ) );
            tracer.Trace( "app exit code: %d\n", g_exit_code );
        }
    }
    catch ( bad_alloc & e )
    {
        printf( "caught exception bad_alloc -- out of RAM. If in RVOS/ARMOS use -h or -m to add RAM. %s\n", e.what() );
    }
    catch ( exception & e )
    {
        printf( "caught a standard execption: %s\n", e.what() );
    }
    catch( ... )
    {
        printf( "caught a generic exception\n" );
    }

    g_consoleConfig.RestoreConsole( false );
    tracer.Shutdown();
    return g_exit_code;
} //main
