/*
#ifdef ARMOS
    This emulates an extemely simple ARM64 Linux-like OS.
    It can load and execute ARM64 apps built with Gnu tools in .elf files targeting Linux.
    Written by David Lee in October 2024
#elif defined( RVOS )
    This emulates an extemely simple RISC-V OS.
    Per https://riscv.org/wp-content/uploads/2017/05/riscv-privileged-v1.10.pdf
    It's an AEE (Application Execution Environment) that exposes an ABI (Application Binary Inferface) for an Application.
    It runs in RISC-V M mode, similar to embedded systems.
    It can load and execute 64-bit RISC-V apps built with Gnu tools in .elf files targeting Linux.
    Written by David Lee in February 2023
    Useful: https://github.com/jart/cosmopolitan/blob/1.0/libc/sysv/consts.sh
#elif defined( M68 )
    This emulates 68000 machines running Linux, CP/M 68k, and the 68000 Visual Simulator Trap 0x15
    how to build a cross compiler for 68000 on Windows:
        http://www.aaldert.com/outrun/gcc-auto.html#:~:text=I've%20made%20the%2068000%20cross%20compiler%20build,have%20MinGW/MSYS%20installed%2C%20and%20have%20an%20internet
    It also runs some cp/m 68k v1.3 binaries. Tested with the C compiler, assembler, and linker. The apps they produce also run
#else
    #error one of ARMOS, RVOS, or M68 must be defined
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

#else
    #include <unistd.h>

    #ifndef OLDGCC      // the several-years-old Gnu C compilers for the RISC-V development boards
#ifndef M68K
        #include <termios.h>
        #include <sys/random.h>
#endif
        #ifdef M68K
            #include <time.h>
        #else
            #include <sys/uio.h>
            #include <dirent.h>
        #endif
        #include <sys/times.h>
        #include <sys/resource.h>
        #if !defined( __APPLE__ ) && !defined( M68K )
            #include <sys/sysinfo.h>
        #endif

        #ifndef M68K
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
    #endif

    struct LINUX_FIND_DATA
    {
        char cFileName[ MAX_PATH ];
    };
#endif

#if defined( _WIN32) || defined( M68K )
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
    #define REG_FORMAT "%lld"
    #define REG_TYPE uint64_t
    #define SIGNED_REG_TYPE int64_t
    #define ACCESS_REG( x ) cpu.regs[ x ]
    #define CPU_IS_LITTLE_ENDIAN true

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
    #define REG_FORMAT "%lld"
    #define REG_TYPE uint64_t
    #define SIGNED_REG_TYPE int64_t
    #define ACCESS_REG( x ) cpu.regs[ x ]
    #define CPU_IS_LITTLE_ENDIAN true

    #define REG_SYSCALL RiscV::a7
    #define REG_RESULT RiscV::a0
    #define REG_ARG0 RiscV::a0
    #define REG_ARG1 RiscV::a1
    #define REG_ARG2 RiscV::a2
    #define REG_ARG3 RiscV::a3
    #define REG_ARG4 RiscV::a4
    #define REG_ARG5 RiscV::a5

#elif defined( M68 )

    #include "m68000.hxx"

    #define CPUClass m68000
    #define ELF_MACHINE_ISA 0x04
    #define APP_NAME "M68"
    #define LOGFILE_NAME L"m68.log"
    #define REG_FORMAT "%d"
    #define REG_TYPE uint32_t
    #define SIGNED_REG_TYPE int32_t
    #define ACCESS_REG( x ) cpu.dregs[ x ].l
    #define CPU_IS_LITTLE_ENDIAN false

    #define REG_SYSCALL 0
    #define REG_RESULT 0
    #define REG_ARG0 1
    #define REG_ARG1 2
    #define REG_ARG2 3
    #define REG_ARG3 4
    #define REG_ARG4 5
    #define REG_ARG5 6

#else

    #error "One of ARMOS, RVOS, or M68 must be defined for compilation"

#endif

CDJLTrace tracer;
ConsoleConfiguration g_consoleConfig;
bool g_compressed_rvc = false;                 // is the app compressed risc-v?
const REG_TYPE g_arg_data_commit = 1024;       // storage spot for command-line arguments and environment variables
const REG_TYPE g_stack_commit = 128 * 1024;    // RAM to allocate for the fixed stack. the top of this has argv data

#ifdef M68
REG_TYPE g_brk_commit = 10 * 1024 * 1024;      // RAM to reserve if the app calls brk to allocate space. 10 meg default
REG_TYPE g_mmap_commit = 10 * 1024 * 1024;     // RAM to reserve if the app mmap to allocate space. 10 meg default
#else
REG_TYPE g_brk_commit = 40 * 1024 * 1024;      // RAM to reserve if the app calls brk to allocate space. 40 meg default
REG_TYPE g_mmap_commit = 40 * 1024 * 1024;     // RAM to reserve if the app mmap to allocate space. 40 meg default
#endif

bool g_terminate = false;                      // has the app asked to shut down?
int g_exit_code = 0;                           // exit code of the app in the vm
vector<uint8_t> memory;                        // RAM for the vm
REG_TYPE g_base_address = 0;                   // vm address of start of memory
REG_TYPE g_execution_address = 0;              // where the program counter starts
REG_TYPE g_brk_offset = 0;                     // offset of brk, initially g_end_of_data
REG_TYPE g_mmap_offset = 0;                    // offset of where mmap allocations start
REG_TYPE g_highwater_brk = 0;                  // highest brk seen during app; peak dynamically-allocated RAM
REG_TYPE g_end_of_data = 0;                    // official end of the loaded app
REG_TYPE g_bottom_of_stack = 0;                // just beyond where brk might move
REG_TYPE g_top_of_stack = 0;                   // argc, argv, penv, aux records sit above this
CMMap g_mmap;                                  // for mmap and munmap system calls
bool g_hostIsLittleEndian = true;              // is the host little endian?

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

#pragma pack( push, 1 )
struct linux_timeval32
{
    uint64_t tv_sec;       // time_t
    uint32_t tv_usec;
};
#pragma pack(pop)

struct linux_tms_syscall
{
    uint64_t tms_utime;
    uint64_t tms_stime;
    uint64_t tms_cutime;
    uint64_t tms_cstime;
};

struct linux_tms_syscall32
{
    uint32_t tms_utime;
    uint32_t tms_stime;
    uint32_t tms_cutime;
    uint32_t tms_cstime;
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

struct linux_rusage_syscall32 {
    struct linux_timeval32 ru_utime; /* user CPU time used */
    struct linux_timeval32 ru_stime; /* system CPU time used */
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
#if !defined( OLDGCC ) && !defined( M68K )
    #define st_atime  st_atim.tv_sec  /* Backward compatibility */
    #define st_mtine  st_mtim.tv_sec
    #define st_ctime  st_ctim.tv_sec
#endif
#endif

    uint64_t   st_mystery_spot_2;
};

uint64_t swap_endian64( uint64_t x )
{
    if ( CPU_IS_LITTLE_ENDIAN != g_hostIsLittleEndian )
        return flip_endian64( x );
    return x;
} //swap_endian64

uint32_t swap_endian32( uint32_t x )
{
    if ( CPU_IS_LITTLE_ENDIAN != g_hostIsLittleEndian )
        return flip_endian32( x );
    return x;
} //swap_endian32

uint16_t swap_endian16( uint16_t x )
{
    if ( CPU_IS_LITTLE_ENDIAN != g_hostIsLittleEndian )
        return flip_endian16( x );
    return x;
} //swap_endian16

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

    void swap_endianness()
    {
        a_type = swap_endian64( a_type );
        a_un.a_val = swap_endian64( a_un.a_val );
    }
};

struct AuxProcessStart32
{
    uint32_t a_type; // AT_xxx ID from elf.h
    union
    {
        uint32_t a_val;
        void * a_ptr;
        void ( * a_fcn )();
    } a_un;

    void swap_endianness()
    {
        a_type = swap_endian32( a_type );
        a_un.a_val = swap_endian32( a_un.a_val );
    }
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

struct ElfHeader32
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
    uint32_t entry_point;
    uint32_t program_header_table;
    uint32_t section_header_table;
    uint32_t flags;
    uint16_t header_size;
    uint16_t program_header_table_size;
    uint16_t program_header_table_entries;
    uint16_t section_header_table_size;
    uint16_t section_header_table_entries;
    uint16_t section_with_section_names;

    void swap_endianness()
    {
        type = swap_endian16( type );
        machine = swap_endian16( machine );
        version = swap_endian32( version );
        entry_point = swap_endian32( entry_point );
        program_header_table = swap_endian32( program_header_table );
        section_header_table = swap_endian32( section_header_table );
        flags = swap_endian32( flags );
        header_size = swap_endian16( header_size );
        program_header_table_size = swap_endian16( program_header_table_size );
        program_header_table_entries = swap_endian16( program_header_table_entries );
        section_header_table_size = swap_endian16( section_header_table_size );
        section_header_table_entries = swap_endian16( section_header_table_entries );
        section_with_section_names = swap_endian16( section_with_section_names );
    }

    void trace()
    {
        printf( "bit width %d\n", bit_width );
        printf( "type %d\n", type );
        printf( "machine %d\n", machine );
    }
};

struct ElfSymbol64
{
    uint32_t name;          // index into the symbol string table
    uint8_t info;           // value of the symbol
    uint8_t other;
    uint16_t shndx;
    uint64_t value;         // address where the symbol resides in memory
    uint64_t size;          // length in memory of the symbol

    const char * show_info() const
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

    const char * show_other() const
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

struct ElfSymbol32
{
    uint32_t name;          // index into the symbol string table
    uint32_t value;         // address where the symbol resides in memory
    uint32_t size;          // length in memory of the symbol
    uint8_t info;           // value of the symbol
    uint8_t other;
    uint16_t shndx;

    void swap_endianness()
    {
        name = swap_endian32( name );
        shndx = swap_endian16( shndx );
        value = swap_endian32( value );
        size = swap_endian32( size );
    } //swap_endianness

    const char * show_info() const
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

    const char * show_other() const
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

    const char * show_type() const
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

    const char * show_flags()
    {
        if ( 7 == flags )
            return "rwe";
        if ( 6 == flags )
            return "rw";
        if ( 5 == flags )
            return "rx";
        if ( 4 == flags )
            return "r";
        if ( 3 == flags )
            return "wx";
        if ( 2 == flags )
            return "w";
        if ( 1 == flags )
            return "x";

        return "";
    }
};

struct ElfProgramHeader32
{
    uint32_t type;
    uint32_t offset_in_image;
    uint32_t virtual_address;
    uint32_t physical_address;
    uint32_t file_size;
    uint32_t memory_size;
    uint32_t flags;
    uint32_t alignment;

    void swap_endianness()
    {
        type = swap_endian32( type );
        offset_in_image = swap_endian32( offset_in_image );
        virtual_address = swap_endian32( virtual_address );
        physical_address = swap_endian32( physical_address );
        file_size = swap_endian32( file_size );
        flags = swap_endian32( flags );
        memory_size = swap_endian32( memory_size );
        alignment = swap_endian32( alignment );
    } //swap_endianness

    const char * show_type() const
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

    const char * show_flags()
    {
        if ( 7 == flags )
            return "rwe";
        if ( 6 == flags )
            return "rw";
        if ( 5 == flags )
            return "rx";
        if ( 4 == flags )
            return "r";
        if ( 3 == flags )
            return "wx";
        if ( 2 == flags )
            return "w";
        if ( 1 == flags )
            return "x";

        return "";
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

    const char * show_type() const
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

    const char * show_flags() const
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

struct ElfSectionHeader32
{
    uint32_t name_offset;
    uint32_t type;
    uint32_t flags;
    uint32_t address;
    uint32_t offset;
    uint32_t size;
    uint32_t link;
    uint32_t info;
    uint32_t address_alignment;
    uint32_t entry_size;

    void swap_endianness()
    {
        name_offset = swap_endian32( name_offset );
        type = swap_endian32( type );
        flags = swap_endian32( flags );
        address = swap_endian32( address );
        offset = swap_endian32( offset );
        size = swap_endian32( size );
        link = swap_endian32( link );
        info = swap_endian32( info );
        address_alignment = swap_endian32( address_alignment );
        entry_size = swap_endian32( entry_size );
    } //swap_endianness

    const char * show_type() const
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

    const char * show_flags() const
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

static void usage( char const * perror = 0 )
{
    g_consoleConfig.RestoreConsole( false );

    if ( 0 != perror )
        printf( "error: %s\n", perror );

    printf( "usage: %s <%s arguments> <executable> <app arguments>\n", APP_NAME, APP_NAME );
    printf( "   arguments:    -e     just show information about the elf executable; don't actually run it\n" );
#ifdef RVOS
    printf( "                 -g     (internal) generate rcvtable.txt then exit\n" );
#endif
    printf( "                 -h:X   # of meg for the heap (brk space). 0..1024 are valid. default is 40\n" );
    printf( "                 -i     if -t is set, also enables instruction tracing with symbols\n" );
    printf( "                 -m:X   # of meg for mmap space. 0..1024 are valid. default is 40. 0 for CP/M\n" );
    printf( "                 -p     shows performance information at app exit\n" );
    printf( "                 -t     enable debug tracing to %ls\n", LOGFILE_NAME );
    printf( "                 -v     used with -e shows verbose information (e.g. symbols)\n" );
    printf( "  %s\n", build_string() );
    exit( 1 );
} //usage

static uint64_t rand64()
{
    uint64_t r = 0;

    for ( int i = 0; i < 7; i++ )
        r = ( r << 15 ) | ( rand() & 0x7FFF );

    return r;
} //rand64

static void backslash_to_slash( char * p )
{
    while ( *p )
    {
        if ( '\\' == *p )
            *p = '/';
        p++;
    }
} //backslash_to_slash

#ifdef _WIN32

static void slash_to_backslash( char * p )
{
    while ( *p )
    {
        if ( '/' == *p )
            *p = '\\';
        p++;
    }
} //slash_to_backslash

static int windows_translate_flags( int flags )
{
    // Translate open() flags from Linux to Windows
    // Microsoft C uses different constants for flags than linux:
    //             msft (win+dos)     linux                                      68000 newlib
    //             --------------     -----                                      ------------
    // 0           O_RDONLY           O_RDONLY                                   O_RDONLY
    // 1           O_WRONLY           O_WRONLY                                   O_WRONLY
    // 2           O_RDRW             O_RDWR                                     O_RDWR
    // 0x8         O_APPEND           n/a                                        O_APPEND
    // 0x10        O_RANDOM           _FMARK / O_SHLOCK / _FSHLOCK / O_SYNC
    // 0x20        O_SEQUENTIAL       _FDEFER / O_EXLOCK / _FEXLOCK
    // 0x40        O_TEMPORARY        O_CREAT
    // 0x80        O_NOINHERIT        O_EXCL
    // 0x100       O_CREAT            n/a
    // 0x200       O_TRUNC            O_TRUNC                                    O_CREAT
    // 0x400       O_EXCL             O_APPEND                                   O_TRUNC
    // 0x800                                                                     O_EXCL
    // 0x2000      O_OBTAINDIR        O_ASYNC                                    O_SYNC
    // 0x4000      O_TEXT             n/a                                        O_NONBLOCK
    // 0x8000      O_BINARY           n/a
    // 0x10100     n/a                O_FSYNC, O_SYNC

    int f = flags & 3; // copy rd/wr/rdrw

    f |= O_BINARY; // this is assumed on Linux systems

#ifdef M68

    if ( 0x200 & flags )
        f |= O_CREAT;

    if ( 0x800 & flags )
        f |= O_EXCL;

    if ( 0x400 & flags )
        f |= O_TRUNC;

    if ( 0x8 & flags )
        f |= O_APPEND;

#else // MacOS + Linux

    if ( 0x40 & flags )
        f |= O_CREAT;

    if ( 0x80 & flags )
        f |= O_EXCL;

    if ( 0x200 & flags )
        f |= O_TRUNC;

    if ( 0x400 & flags )
        f |= O_APPEND;

#endif

    tracer.Trace( "  flags translated from linux/macos/68000 %x to Microsoft %x\n", flags, f );
    return f;
} //windows_translate_flags

static uint32_t epoch_days( uint16_t y, uint16_t m, uint16_t d ) // taken from https://blog.reverberate.org/2020/05/12/optimizing-date-algorithms.html
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

static uint64_t SystemTime_to_esecs( SYSTEMTIME & st )
{
    // takes a Windows system time and returns linux epoch seconds
    uint32_t edays = epoch_days( st. wYear, st.wMonth, st.wDay );
    uint32_t secs = ( st.wHour * 3600 ) + ( st.wMinute * 60 ) + st.wSecond;
    return ( edays * 24ull * 3600ull ) + secs;
} //SystemTime_to_esecs

static int fill_pstat_windows( int descriptor, struct stat_linux_syscall * pstat, const char * path )
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

static const char * get_clockid( clockid_t clockid )
{
    if ( clockid < _countof( clockids ) )
        return clockids[ clockid ];
    return "unknown";
} //get_clockid

high_resolution_clock::time_point g_tAppStart;

static int msc_clock_gettime( clockid_t clockid, struct timespec_syscall * tv )
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

#ifdef M68K
extern "C" int clock_gettime( clockid_t id, struct timespec * res );
#endif

static int gettimeofday( linux_timeval * tp )
{
#if 1

#ifdef _WIN32
    struct timespec_syscall tv;
    msc_clock_gettime( CLOCK_REALTIME, &tv );
    tp->tv_sec = tv.tv_sec;
    tp->tv_usec = tv.tv_nsec / 1000;
#else
    struct timespec local_ts; // this varies in size from platform to platform
    int result = clock_gettime( CLOCK_REALTIME, & local_ts );
    tp->tv_sec = local_ts.tv_sec;
    tp->tv_usec = local_ts.tv_nsec / 1000;
#endif

#else
    // the older GCC chrono implementations are built on time(), which this calls but
    // has only second resolution. So the microseconds returned here are lost.
    // All other C++ implementations do the right thing.

    namespace sc = std::chrono;
    sc::system_clock::duration d = sc::system_clock::now().time_since_epoch();
    sc::seconds s = sc::duration_cast<sc::seconds>( d );
    tp->tv_sec = s.count();
    tp->tv_usec = sc::duration_cast<sc::microseconds>( d - s ).count();
#endif
    return 0;
} //gettimeofday

#ifdef M68
static int linux_translate_flags( int flags )
{
    int f = flags & 3; // copy rd/wr/rdrw

    if ( 0x200 & flags )
        f |= 0x40; // O_CREAT

    if ( 0x800 & flags )
        f |= 0x80; // O_EXCL

    if ( 0x400 & flags )
        f |= 0x200; // O_TRUNC

    if ( 0x8 & flags )
        f |= 0x400; // O_APPEND

    tracer.Trace( "  flags translated from 68000 %x to linux %x\n", flags, f );
    return f;
} //linux_translate_flags
#endif

#ifdef __APPLE__

// For each of these flag fields o/i/l/c this code translates the subset of the values actually used in the apps
// I validated. There are certainly other cases that are still broken (though many aren't implemented in MacOS,
// so translating them wouldn't have any impact). If anyone understands the historical reasons for the differences
// in these flags I'd love to hear about it. Why did coders knowngly pick different constant values?

static tcflag_t map_termios_oflag_linux_to_macos( tcflag_t f )
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

static tcflag_t map_termios_oflag_macos_to_linux( tcflag_t f )
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

static tcflag_t map_termios_iflag_linux_to_macos( tcflag_t f )
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

static tcflag_t map_termios_iflag_macos_to_linux( tcflag_t f )
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

static tcflag_t map_termios_lflag_linux_to_macos( tcflag_t f )
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

static tcflag_t map_termios_lflag_macos_to_linux( tcflag_t f )
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

static tcflag_t map_termios_cflag_linux_to_macos( tcflag_t f )
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

static tcflag_t map_termios_cflag_macos_to_linux( tcflag_t f )
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
#if defined(__ARM_32BIT_STATE) || defined(__ARM_64BIT_STATE) || defined(__riscv) || defined( __amd64 )
    static int linux_swap_riscv64_arm_dir_open_flags( int flags )
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

        #if defined( O_DIRECT ) && defined( O_DIRECTORY )
            tracer.Trace( "  O_DIRECT %#x, O_DIRECTORY %#x\n", O_DIRECT, O_DIRECTORY );
        #endif
        tracer.Trace( "  mapped from flags %#x to flags %#x\n", flags, r );
        return r;
    } //linux_swap_riscv64_arm_dir_open_flags
#endif
#endif // !APPLE

struct SysCall
{
    const char * name;
    uint32_t id;
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
    { "emulator_sys_print_char", emulator_sys_print_char },
};

// Use custom versions of bsearch and qsort to get consistent behavior across platforms.
// That consistency enables identical instruction trace log files across platforms for debugging.

const void * my_bsearch( const void * key, const void * vbase, size_t num, unsigned width, int (*compare)( const void * a, const void * b ) )
{
    const char * base = (const char *) vbase;
    int i = 0;
    int j = (int) num - 1;
    do
    {
        int k = ( j + i ) / 2;
        const char * here = base + width*k;
        int cmp = ( *compare )( key, here );
        if ( 0 == cmp )
        {
            while ( ( here > base ) && ( ( *compare )( key, here - width ) == 0 ) )
                here -= width;
            return here;
        }

        if ( cmp < 0 )
            j = k - 1;
        else
            i = k + 1;
      } while ( j >= i );

   return 0;
} //my_bsearch

void memswap( char * a, char * b, unsigned len )
{
    char t;
    for ( unsigned x = 0; x < len; x++ )
    {
        t = *a;
        *a = *b;
        *b = t;
        a++;
        b++;
    }
} //memswap

// adapted from powerc
void my_qsort( void * vbase, size_t num, unsigned width, int (*compare)( const void * a, const void * b ) )
{
    char * base = (char *) vbase;
    char * max;
    char * last = max = base + ( num - 1 ) * width;
    char * first = base;
    char * key = base + width * ( num >> 1 );
    do
    {
        while ( ( *compare )( first, key ) < 0 )
            first += width;
        while ( ( *compare )( key, last ) < 0 )
            last -= width;

        if ( first <= last )
        {
            if (first != last)
            {
                memswap( first, last, width );
                if ( first == key )
                    key = last;
                else if ( last == key )
                    key = first;
             }
             first += width;
             last -= width;
        }
    } while ( first <= last );

    if ( base < last )
        my_qsort( base, ( last - base ) / width + 1, width, compare );
    if ( first < max)
        my_qsort( first, ( max - first ) / width + 1, width, compare );
} //my_qsort

static int syscall_find_compare( const void * a, const void * b )
{
    SysCall & sa = * (SysCall *) a;
    SysCall & sb = * (SysCall *) b;

    if ( sa.id > sb.id )
        return 1;

    if ( sa.id < sb.id )
        return -1;

    return 0;
} //syscall_find_compare

static const char * lookup_syscall( uint32_t x )
{
#ifndef NDEBUG
    // ensure they're sorted
    for ( size_t i = 0; i < _countof( syscalls ) - 1; i++ )
        assert( syscalls[ i ].id < syscalls[ i + 1 ].id );
#endif

    SysCall key = { 0, x };
    SysCall * presult = (SysCall *) my_bsearch( &key, syscalls, _countof( syscalls ), sizeof( key ), syscall_find_compare );

    if ( 0 != presult )
        return presult->name;

    return "unknown";
} //lookup_syscall

static void update_result_errno( CPUClass & cpu, SIGNED_REG_TYPE result )
{
    if ( result >= 0 || result <= -4096 ) // syscalls like write() return positive values to indicate success.
    {
        tracer.Trace( "  syscall success, returning %lld = %#llx\n", result, result );
        ACCESS_REG( REG_RESULT ) = (REG_TYPE) result;
    }
    else
    {
        tracer.Trace( "  returning negative errno: %d\n", -errno );
        ACCESS_REG( REG_RESULT ) = (REG_TYPE) -errno; // it looks like the g++ runtime copies the - of this value to errno
    }
} //update_result_errno

#ifdef M68K
extern "C" long syscall( long number, ... );
#endif

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
    #if !defined( OLDGCC ) && !defined( M68K )
        static DIR * g_FindFirst = 0;
        static uint64_t g_FindFirstDescriptor = -1;
    #endif
#endif

    // Linux syscalls support up to 6 arguments

    if ( tracer.IsEnabled() )
#ifdef M68
        tracer.Trace( "syscall %s %x = %d, arg0 %x, arg1 %x, arg2 %x, arg3 %x, arg4 %x, arg5 %x\n",
                      lookup_syscall( ACCESS_REG( REG_SYSCALL ) ), ACCESS_REG( REG_SYSCALL ), ACCESS_REG( REG_SYSCALL ),
                      ACCESS_REG( REG_ARG0 ), ACCESS_REG( REG_ARG1 ), ACCESS_REG( REG_ARG2 ), ACCESS_REG( REG_ARG3 ),
                      ACCESS_REG( REG_ARG4 ), ACCESS_REG( REG_ARG5 ) );
#else
        tracer.Trace( "syscall %s %llx = %lld, arg0 %llx, arg1 %llx, arg2 %llx, arg3 %llx, arg4 %llx, arg5 %llx\n",
                      lookup_syscall( (uint32_t) ACCESS_REG( REG_SYSCALL ) ), ACCESS_REG( REG_SYSCALL ), ACCESS_REG( REG_SYSCALL ),
                      ACCESS_REG( REG_ARG0 ), ACCESS_REG( REG_ARG1 ), ACCESS_REG( REG_ARG2 ), ACCESS_REG( REG_ARG3 ),
                      ACCESS_REG( REG_ARG4 ), ACCESS_REG( REG_ARG5 ) );
#endif

    switch ( ACCESS_REG( REG_SYSCALL ) )
    {
        case emulator_sys_exit: // exit
        case SYS_exit:
        case SYS_exit_group:
        case SYS_tgkill:
        {
            g_terminate = true;
            cpu.end_emulation();
            g_exit_code = (int) ACCESS_REG( REG_ARG0 );
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
            printf( REG_FORMAT, ACCESS_REG( REG_ARG0 ) );
            fflush( stdout );
            update_result_errno( cpu, 0 );
            break;
        }
        case emulator_sys_print_char:
        {
            if ( 12 != ACCESS_REG( REG_ARG0 ) )
            {
                printf( "%c", (uint8_t) ACCESS_REG( REG_ARG0 ) );
                fflush( stdout );
            }
            update_result_errno( cpu, 0 );
            break;
        }
        case emulator_sys_print_text: // print asciiz in a0
        {
            tracer.Trace( "  syscall command print string '%s'\n", (char *) cpu.getmem( ACCESS_REG( REG_ARG0 ) ) );
            printf( "%s", (char *) cpu.getmem( ACCESS_REG( REG_ARG0 ) ) );
            fflush( stdout );
            update_result_errno( cpu, 0 );
            break;
        }
        case emulator_sys_get_datetime: // writes local date/time into string pointed to by a0
        {
            char * pdatetime = (char *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
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

            REG_TYPE original = ACCESS_REG( REG_ARG0 );
            tracer.Trace( "  address in vm space: %llx == %llu\n", original, original );
            char * pin = (char *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
            size_t size = (size_t) ACCESS_REG( REG_ARG1 );
            uint64_t pout = 0;
#ifdef _WIN32
            char * poutwin32 = _getcwd( acPath, sizeof( acPath ) );
            if ( poutwin32 )
            {
                tracer.Trace( "  acPath: '%s', poutwin32: '%s'\n", acPath, poutwin32 );
                backslash_to_slash( poutwin32 );
                pout = original;
                strcpy( pin, poutwin32 + 2 ); // get past C:
            }
            else
                tracer.Trace( "  _getcwd failed on win32, error %d\n", (int) errno );

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
            int fd = (int) ACCESS_REG( REG_ARG0 );
            int op = (int) ACCESS_REG( REG_ARG1 );

            if ( 1 == op ) // F_GETFD
                ACCESS_REG( REG_RESULT ) = 1; // FD_CLOEXEC
            else
                tracer.Trace( "unhandled SYS_fcntl operation %d\n", op );
            break;
        }
        case SYS_clock_nanosleep:
        {
            clockid_t clockid = (clockid_t) ACCESS_REG( REG_ARG0 );
            int flags = (int) ACCESS_REG( REG_ARG1 );
            tracer.Trace( "  nanosleep id %d flags %x\n", clockid, flags );

            const struct timespec_syscall * request = (const struct timespec_syscall *) cpu.getmem( ACCESS_REG( REG_ARG2 ) );
            struct timespec_syscall * remain = 0;
            if ( 0 != ACCESS_REG( REG_ARG3 ) )
                remain = (struct timespec_syscall *) cpu.getmem( ACCESS_REG( REG_ARG3 ) );

            struct timespec_syscall local_request = * request;

#if defined( M68 ) && !defined( M68K )
            local_request.tv_sec = flip_endian64( local_request.tv_sec );
            local_request.tv_nsec = flip_endian64( local_request.tv_nsec );
#endif

            uint64_t ms = local_request.tv_sec * 1000 + local_request.tv_nsec / 1000000;
            tracer.Trace( "  nanosleep sec %llu, nsec %llu == %llu ms\n", local_request.tv_sec, local_request.tv_nsec, ms );
            sleep_ms( ms ); // ignore remain argument because there are no signals to wake the thread
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
            int descriptor = (int) ACCESS_REG( REG_ARG0 );
            int result = 0;

#ifdef _WIN32
            struct stat_linux_syscall local_stat = {0};
            result = fill_pstat_windows( descriptor, & local_stat, 0 );
            if ( 0 == result )
            {
                size_t cbStat = sizeof( struct stat_linux_syscall );
                tracer.Trace( "  sizeof stat_linux_syscall: %zd\n", cbStat );
                assert( 128 == cbStat );  // 128 is the size of the stat struct this syscall on RISC-V Linux
                memcpy( cpu.getmem( ACCESS_REG( REG_ARG1 ) ), & local_stat, cbStat );
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

                struct stat_linux_syscall * pout = (struct stat_linux_syscall *) cpu.getmem( ACCESS_REG( REG_ARG1 ) );
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
#elif defined( OLDGCC ) || defined( M68K )
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
            linux_timeval * ptimeval = (linux_timeval *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
            int result = 0;
            if ( 0 != ptimeval )
            {
                linux_timeval tv = {0};
                result = gettimeofday( &tv );

                if ( 0 == result )
                {
#ifdef M68
                    linux_timeval32 * ptimeval32 = (linux_timeval32 *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
                    ptimeval32->tv_sec = tv.tv_sec;
                    ptimeval32->tv_usec = (uint32_t) tv.tv_usec;
#ifndef M68K
                    ptimeval32->tv_sec = swap_endian64( ptimeval32->tv_sec );
                    ptimeval32->tv_usec = swap_endian32( ptimeval32->tv_usec );
#endif

                    tracer.Trace( "    reg_arg0 %#x\n", ACCESS_REG( REG_ARG0 ) );
                    tracer.Trace( "    tv.tv_sec %#llx, swapped %#llx\n", tv.tv_sec, ptimeval32->tv_sec );
                    tracer.Trace( "    tv_usec %#lx, swapped %#lx\n", swap_endian32( ptimeval32->tv_usec ), ptimeval32->tv_usec );
#else
                    ptimeval->tv_sec = tv.tv_sec;
                    ptimeval->tv_usec = tv.tv_usec;
#endif
                }
            }

            tracer.Trace( "  returning result %d\n", result );
            ACCESS_REG( REG_RESULT ) = result;
            break;
        }
        case SYS_lseek:
        {
            tracer.Trace( "  syscall command SYS_lseek\n" );
            int descriptor = (int) ACCESS_REG( REG_ARG0 );
            int offset = (int) ACCESS_REG( REG_ARG1 );
            int origin = (int) ACCESS_REG( REG_ARG2 );

            long result = lseek( descriptor, offset, origin );
            update_result_errno( cpu, result );
            break;
        }
        case SYS_read:
        {
            int descriptor = (int) ACCESS_REG( REG_ARG0 );
            void * buffer = cpu.getmem( ACCESS_REG( REG_ARG1 ) );
            unsigned buffer_size = (unsigned) ACCESS_REG( REG_ARG2 );
            tracer.Trace( "  syscall command SYS_read. descriptor %d, buffer %llx, buffer_size %u\n", descriptor, ACCESS_REG( REG_ARG1 ), buffer_size );

            if ( 0 == descriptor ) //&& 1 == buffer_size )
            {
#ifdef _WIN32
                int r = g_consoleConfig.linux_getch();
#else
                int r = g_consoleConfig.portable_getch();
#endif
                * (char *) buffer = (char) r;
                ACCESS_REG( REG_RESULT ) = 1;
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
            tracer.Trace( "  syscall command SYS_write. fd %lld, buf %llx, count %lld\n", (uint64_t) ACCESS_REG( REG_ARG0 ), (uint64_t) ACCESS_REG( REG_ARG1 ), (uint64_t) ACCESS_REG( REG_ARG2 ) );

            int descriptor = (int) ACCESS_REG( REG_ARG0 );
            uint8_t * p = (uint8_t *) cpu.getmem( ACCESS_REG( REG_ARG1 ) );
            REG_TYPE count = ACCESS_REG( REG_ARG2 );

            tracer.Trace( "    descriptor %d, pdata %p, count %u\n", descriptor, p, count );

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
                update_result_errno( cpu, (REG_TYPE) written );
            }
            break;
        }
        case SYS_open:
        {
            tracer.Trace( "  syscall command SYS_open\n" );

            // a0: asciiz string of file to open. a1: flags. a2: mode

            const char * pname = (const char *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
            int flags = (int) ACCESS_REG( REG_ARG1 );
            int mode = (int) ACCESS_REG( REG_ARG2 );

            tracer.Trace( "  open flags %x, mode %x, file %s\n", flags, mode, pname );

#ifdef _WIN32
            flags = windows_translate_flags( flags );
#endif

#if defined(M68) && !defined(M68K)
            flags = linux_translate_flags( flags );
#endif

            int descriptor = open( pname, flags, mode );
            tracer.Trace( "  descriptor: %d\n", descriptor );
            update_result_errno( cpu, descriptor );
            break;
        }
        case SYS_close:
        {
            tracer.Trace( "  syscall command SYS_close\n" );
            int descriptor = (int) ACCESS_REG( REG_ARG0 );

            if ( descriptor >=0 && descriptor <= 3 )
            {
                // built-in handle stdin, stdout, stderr -- ignore
                ACCESS_REG( REG_RESULT ) = 0;
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
                    update_result_errno( cpu, 0 );
                    break;
                }
                else if ( timebaseFrequencyDescriptor == descriptor || osreleaseDescriptor == descriptor )
                {
                    update_result_errno( cpu, 0 );
                    break;
                }
                else
#else
    #if !defined( OLDGCC ) && !defined( M68K )
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
            int result = 0;
            REG_TYPE descriptor = ACCESS_REG( REG_ARG0 );
            uint8_t * pentries = (uint8_t *) cpu.getmem( ACCESS_REG( REG_ARG1 ) );
            REG_TYPE count = ACCESS_REG( REG_ARG2 );
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
        #if !defined( OLDGCC ) && !defined( M68K )
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
            REG_TYPE original = g_brk_offset;
            REG_TYPE ask = ACCESS_REG( REG_ARG0 );
            if ( 0 == ask )
                ACCESS_REG( REG_RESULT ) = cpu.get_vm_address( g_brk_offset );
            else
            {
                REG_TYPE ask_offset = ask - g_base_address;
                tracer.Trace( "  ask_offset %llx, g_end_of_data %llx, bottom_of_stack %llx\n", (uint64_t) ask_offset, (uint64_t) g_end_of_data, (uint64_t) g_bottom_of_stack );

                if ( ask_offset >= g_end_of_data && ask_offset < g_bottom_of_stack )
                {
                    g_brk_offset = cpu.getoffset( ask );
                    if ( g_brk_offset > g_highwater_brk )
                        g_highwater_brk = g_brk_offset;
                }
                else
                {
                    tracer.Trace( "  allocation request was too large, failing it by returning current brk\n" );
                    ACCESS_REG( REG_RESULT ) = cpu.get_vm_address( g_brk_offset );
                }
            }

            tracer.Trace( "  SYS_brk. ask %llx, current brk %llx, new brk %llx, result in return register %llx\n",
                          (uint64_t) ask, (uint64_t) original, (uint64_t) g_brk_offset, (uint64_t) ACCESS_REG( REG_RESULT ) );
            break;
        }
        case SYS_munmap:
        {
            REG_TYPE address = ACCESS_REG( REG_ARG0 );
            REG_TYPE length = ACCESS_REG( REG_ARG1 );
            length = round_up( length, (REG_TYPE) 4096 );

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
            REG_TYPE address = ACCESS_REG( REG_ARG0 );
            REG_TYPE old_length = ACCESS_REG( REG_ARG1 );
            REG_TYPE new_length = ACCESS_REG( REG_ARG2 );
            int flags = (int) ACCESS_REG( REG_ARG3 );

            if ( 0 != ( new_length & 0xfff ) )
            {
                tracer.Trace( "  warning: mremap allocation new length isn't 4k-page aligned\n" );
                new_length = round_up( new_length, (REG_TYPE) 4096 );
            }

            old_length = round_up( old_length, (REG_TYPE) 4096 );

            // flags: MREMAP_MAYMOVE = 1, MREMAP_FIXED = 2, MREMAP_DONTUNMAP = 3. Ignore them all

            SIGNED_REG_TYPE result = (SIGNED_REG_TYPE) g_mmap.resize( address, old_length, new_length, ( 1 == flags ) );
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
            ACCESS_REG( REG_RESULT ) = (REG_TYPE) rand64();
            break;
        }
        case emulator_sys_print_double: // print_double in a0
        {
            tracer.Trace( "  syscall command print double in a0\n" );
            double d;
            memcpy( &d, &ACCESS_REG( REG_ARG0 ), sizeof d );
            printf( "%lf", d );
            fflush( stdout );
            update_result_errno( cpu, 0 );
            break;
        }
        case emulator_sys_trace_instructions:
        {
            tracer.Trace( "  syscall command trace_instructions %d\n", ACCESS_REG( REG_ARG0 ) );
#if !defined( _WIN32 ) && !defined( __APPLE__ ) // only on Linux can there be a parent emulator
            uint64_t v = ACCESS_REG( REG_ARG0 );
            syscall( 0x2002, 0 != v );
#endif
            ACCESS_REG( REG_RESULT ) = cpu.trace_instructions( 0 != ACCESS_REG( REG_ARG0 ) );
            break;
        }
        case SYS_mmap:
        {
            // The gnu c runtime is ok with this failing -- it just allocates memory instead probably assuming it's an embedded system.
            // Same for the gnu runtime with Rust.
            // But golang actually needs this to succeed and return the requested address and this code doesn't support that.

            REG_TYPE addr = ACCESS_REG( REG_ARG0 );
            size_t length = ACCESS_REG( REG_ARG1 );
            int prot = (int) ACCESS_REG( REG_ARG2 );
            int flags = (int) ACCESS_REG( REG_ARG3 );
            int fd = (int) ACCESS_REG( REG_ARG4 );
            size_t offset = (size_t) ACCESS_REG( REG_ARG5 );
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
                        SIGNED_REG_TYPE result = (SIGNED_REG_TYPE) g_mmap.allocate( length );
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

            int directory = (int) ACCESS_REG( REG_ARG0 );
#ifdef __APPLE__
            if ( -100 == directory )
                directory = -2; // Linux vs. MacOS
#endif
            const char * pname = (const char *) cpu.getmem( ACCESS_REG( REG_ARG1 ) );
            int flags = (int) ACCESS_REG( REG_ARG2 );
            int mode = (int) ACCESS_REG( REG_ARG3 );
            int64_t descriptor = 0;

            tracer.Trace( "  open dir %d, flags %x, mode %x, file '%s'\n", directory, flags, mode, pname );
            //tracer.Trace( "  O_DIRECT %#x, O_DIRECTORY %#x\n", O_DIRECT, O_DIRECTORY );

            if ( !strcmp( pname, "/proc/device-tree/cpus/timebase-frequency" ) )
            {
                descriptor = timebaseFrequencyDescriptor;
                update_result_errno( cpu, (int) descriptor );
                break;
            }

            if ( !strcmp( pname, "/proc/sys/kernel/osrelease" ) )
            {
                descriptor = osreleaseDescriptor;
                update_result_errno( cpu, (int) descriptor );
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
            {
#ifdef M68
                if ( O_CREAT & flags )
                    mode = _S_IREAD | _S_IWRITE; // m68k passes user-related flags that conflict with these flags
#endif
                descriptor = _open( pname, flags, mode );
            }
#else

// if it's rvos running on Arm or armos running on risc-v, swap O_DIRECT and O_DIRECTORY

#ifndef __APPLE__

#ifdef RVOS
    #if defined(__ARM_32BIT_STATE) || defined(__ARM_64BIT_STATE)
            flags = linux_swap_riscv64_arm_dir_open_flags( flags );
    #endif // ARM32 or ARM64
#elif defined( ARMOS )
    #if defined (__riscv) || defined( __amd64 )
            flags = linux_swap_riscv64_arm_dir_open_flags( flags );
    #endif // __riscv
#endif

#endif

#if defined(M68) && !defined(__APPLE__) && !defined(M68K)  // macOS and 68000 share the same open flags and are different than generic linux
            flags = linux_translate_flags( flags );
#endif

            tracer.Trace( "  final directory %d, flags %#llx, mode %x passed to openat\n", directory, flags, mode );
            descriptor = openat( directory, pname, flags, mode );
#endif
            update_result_errno( cpu, (int) descriptor );
            break;
        }
        case SYS_sysinfo:
        {
#if defined(_WIN32) || defined(__APPLE__) || defined( M68K )
            errno = EACCES;
            update_result_errno( cpu, -1 );
#else
            int result = sysinfo( (struct sysinfo *) cpu.getmem( ACCESS_REG( REG_ARG0 ) ) );
            update_result_errno( cpu, result );
#endif
            break;
        }
        case SYS_newfstatat:
        {
            const char * path = (char *) cpu.getmem( ACCESS_REG( REG_ARG1 ) );
            tracer.Trace( "  syscall command SYS_newfstatat, id %lld, path '%s', flags %llx\n", ACCESS_REG( REG_ARG0 ), path, ACCESS_REG( REG_ARG3 ) );
            int descriptor = (int) ACCESS_REG( REG_ARG0 );
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
                memcpy( cpu.getmem( ACCESS_REG( REG_ARG2 ) ), & local_stat, cbStat );
                tracer.Trace( "  file size in bytes: %zd, offsetof st_size: %zd\n", local_stat.st_size, offsetof( struct stat_linux_syscall, st_size ) );
            }
            else
                tracer.Trace( "  fill_pstat_windows failed\n" );
#else
            tracer.Trace( "  sizeof struct stat: %zd\n", sizeof( struct stat ) );
            struct stat local_stat = {0};
            struct stat_linux_syscall local_stat_syscall = {0};
            int flags = (int) ACCESS_REG( REG_ARG3 );
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

                struct stat_linux_syscall * pout = (struct stat_linux_syscall *) cpu.getmem( ACCESS_REG( REG_ARG2 ) );
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
#ifndef M68K
                pout->st_atim = local_stat.st_atim;
                pout->st_mtim = local_stat.st_mtim;
                pout->st_ctim = local_stat.st_ctim;
#endif
#endif
                tracer.Trace( "  file size %zd, isdir %s\n", local_stat.st_size, S_ISDIR( local_stat.st_mode ) ? "yes" : "no" );
            }
#endif
            update_result_errno( cpu, result );
            break;
        }
        case SYS_chdir:
        {
            const char * path = (const char *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
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
            int directory = (int) ACCESS_REG( REG_ARG0 );
            const char * path = (const char *) cpu.getmem( ACCESS_REG( REG_ARG1 ) );
#ifdef _WIN32 // on windows ignore the directory and mode arguments
            tracer.Trace( "  syscall command SYS_mkdirat path %s\n", path );
            int result = _mkdir( path );
#else
#ifdef __APPLE__
            if ( -100 == directory )
                directory = -2;
#endif
            mode_t mode = (mode_t) ACCESS_REG( REG_ARG2 );
            tracer.Trace( "  syscall command SYS_mkdirat dir %d, path %s, mode %x\n", directory, path, mode );
            int result = mkdirat( directory, path, mode );
#endif
            update_result_errno( cpu, result );
            break;
        }
        case SYS_unlinkat:
        {
            int directory = (int) ACCESS_REG( REG_ARG0 );
            const char * path = (const char *) cpu.getmem( ACCESS_REG( REG_ARG1 ) );
            int flags = (int) ACCESS_REG( REG_ARG2 );
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
            const char * path = (const char *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
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
            struct utsname_syscall * pname = (struct utsname_syscall *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
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
            int who = (int) ACCESS_REG( REG_ARG0 );
#ifdef M68
            struct linux_rusage_syscall32 *prusage = (struct linux_rusage_syscall32 *) cpu.getmem( ACCESS_REG( REG_ARG1 ) );
#else
            struct linux_rusage_syscall *prusage = (struct linux_rusage_syscall *) cpu.getmem( ACCESS_REG( REG_ARG1 ) );
            memset( prusage, 0, sizeof( struct linux_rusage_syscall ) );
#endif

            if ( 0 == who ) // RUSAGE_SELF
            {
#ifdef _WIN32
                FILETIME ftCreation, ftExit, ftKernel, ftUser;
                if ( GetProcessTimes( GetCurrentProcess(), &ftCreation, &ftExit, &ftKernel, &ftUser ) )
                {
                    uint64_t utotal = ( ( (uint64_t) ftUser.dwHighDateTime << 32 ) + ftUser.dwLowDateTime ) / 10; // 100ns to microseconds
#ifdef M68
                    prusage->ru_utime.tv_sec = utotal / 1000000;
                    prusage->ru_utime.tv_usec = (uint32_t) ( utotal % 1000000 );
#ifndef M68K
                    prusage->ru_utime.tv_sec = flip_endian64( prusage->ru_utime.tv_sec );
                    prusage->ru_utime.tv_usec = flip_endian32( prusage->ru_utime.tv_usec );
#endif

#else
                    prusage->ru_utime.tv_sec = utotal / 1000000;
                    prusage->ru_utime.tv_usec = utotal % 1000000;
#endif
                    uint64_t stotal = ( ( (uint64_t) ftKernel.dwHighDateTime << 32 ) + ftKernel.dwLowDateTime ) / 10;
#ifdef M68
                    prusage->ru_stime.tv_sec = stotal / 1000000;
                    prusage->ru_stime.tv_usec = (uint32_t) ( stotal % 1000000 );
#ifndef M68K
                    prusage->ru_stime.tv_sec = flip_endian64( prusage->ru_stime.tv_sec );
                    prusage->ru_stime.tv_usec = flip_endian32( prusage->ru_stime.tv_usec );
#endif

#else
                    prusage->ru_stime.tv_sec = stotal / 1000000;
                    prusage->ru_stime.tv_usec = stotal % 1000000;
#endif
                }
                else
                    tracer.Trace( "  unable to GetProcessTimes, error %d\n", GetLastError() );
#else
                struct rusage local_rusage;
                getrusage( who, &local_rusage ); // on 32-bit systems fields are 32 bit
#ifdef M68
                prusage->ru_utime.tv_sec = local_rusage.ru_utime.tv_sec;
                prusage->ru_utime.tv_usec = (uint32_t) local_rusage.ru_utime.tv_usec;
                prusage->ru_stime.tv_sec = local_rusage.ru_stime.tv_sec;
                prusage->ru_stime.tv_usec = (uint32_t) local_rusage.ru_stime.tv_usec;
#ifndef M68K
                prusage->ru_utime.tv_sec = flip_endian64( prusage->ru_utime.tv_sec );
                prusage->ru_utime.tv_usec = flip_endian32( prusage->ru_utime.tv_usec );
                prusage->ru_stime.tv_sec = flip_endian64( prusage->ru_stime.tv_sec );
                prusage->ru_stime.tv_usec = flip_endian32( prusage->ru_stime.tv_usec );
#endif
#else
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
#endif
            }
            else
                tracer.Trace( "  unsupported request for who %u\n", who );

            update_result_errno( cpu, 0 );
            break;
        }
        case SYS_futex:
        {
            if ( !cpu.is_address_valid( ACCESS_REG( REG_ARG0 ) ) ) // sometimes it's malformed on arm64. not sure why yet.
            {
                tracer.Trace( "futex pointer in reg 0 is malformed\n" );
                ACCESS_REG( REG_RESULT ) = 0;
                break;
            }

            uint32_t * paddr = (uint32_t *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
            int futex_op = (int) ACCESS_REG( REG_ARG1 ) & (~128); // strip "private" flags
            uint32_t value = (uint32_t) ACCESS_REG( REG_ARG2 );

            tracer.Trace( "  futex all paddr %p (%d), futex_op %d, val %d\n", paddr, ( 0 == paddr ) ? -666 : *paddr, futex_op, value );

            if ( 0 == futex_op ) // FUTEX_WAIT
            {
                if ( *paddr != value )
                    ACCESS_REG( REG_RESULT ) = 11; // EAGAIN
                else
                    ACCESS_REG( REG_RESULT ) = 0;
            }
            else if ( 1 == futex_op ) // FUTEX_WAKE
                ACCESS_REG( REG_RESULT ) = 0;
            else
                ACCESS_REG( REG_RESULT ) = (REG_TYPE) -1; // fail this until/unless there is a real-world use
            break;
        }
#ifndef M68 // lots of 64/32 interop issues with this
        case SYS_writev:
        {
            int descriptor = (int) ACCESS_REG( REG_ARG0 );
            const struct iovec * pvec = (const struct iovec *) cpu.getmem( ACCESS_REG( REG_ARG1 ) );
            if ( 1 == descriptor || 2 == descriptor )
                tracer.Trace( "  desc %d: writing '%.*s'\n", descriptor, pvec->iov_len, cpu.getmem( (REG_TYPE) pvec->iov_base ) );

#ifdef _WIN32
            int64_t result = write( descriptor, cpu.getmem( (uint64_t) pvec->iov_base ), (unsigned) pvec->iov_len );
#else
            struct iovec vec_local;
            vec_local.iov_base = cpu.getmem( (uint64_t) pvec->iov_base );
            vec_local.iov_len = pvec->iov_len;
            tracer.Trace( "  write length: %u to descriptor %d at addr %p\n", pvec->iov_len, descriptor, vec_local.iov_base );
            int64_t result = writev( descriptor, &vec_local, ACCESS_REG( REG_ARG2 ) );
#endif
            update_result_errno( cpu, result );
            break;
        }
#endif
        case SYS_clock_gettime:
        {
            clockid_t cid = (clockid_t) ACCESS_REG( REG_ARG0 );
            struct timespec_syscall * ptimespec = (struct timespec_syscall *) cpu.getmem( ACCESS_REG( REG_ARG1 ) );
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

#if defined(M68) && !defined(M68K)
            ptimespec->tv_sec = flip_endian64( ptimespec->tv_sec );
            ptimespec->tv_nsec = flip_endian64( ptimespec->tv_nsec );
#endif

            tracer.Trace( "  tv_sec %llx, tv_nsec %llx\n", ptimespec->tv_sec, ptimespec->tv_nsec );
            update_result_errno( cpu, result );
            break;
        }
        case SYS_fdatasync:
        {
            int descriptor = (int) ACCESS_REG( REG_ARG0 );

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

#ifdef M68
            struct linux_tms_syscall32 * ptms = ( 0 != ACCESS_REG( REG_ARG0 ) ) ? (struct linux_tms_syscall32 *) cpu.getmem( ACCESS_REG( REG_ARG0 ) ) : 0;
#else
            struct linux_tms_syscall * ptms = ( 0 != ACCESS_REG( REG_ARG0 ) ) ? (struct linux_tms_syscall *) cpu.getmem( ACCESS_REG( REG_ARG0 ) ) : 0;
#endif

            if ( 0 != ptms ) // 0 is legal if callers just want the return value
            {
                memset( ptms, 0, sizeof ( *ptms ) );
#ifdef _WIN32
                FILETIME ftCreation, ftExit, ftKernel, ftUser;
                if ( GetProcessTimes( GetCurrentProcess(), &ftCreation, &ftExit, &ftKernel, &ftUser ) )
                {
                    ptms->tms_utime = ( ( (uint64_t) ftUser.dwHighDateTime << 32) + ftUser.dwLowDateTime ) / 100000; // 100ns to hundredths of a second
                    ptms->tms_stime = ( ( (uint64_t) ftKernel.dwHighDateTime << 32 ) + ftKernel.dwLowDateTime ) / 100000;
#if defined( M68 ) && !defined( M68K )
                    ptms->tms_utime = flip_endian32( ptms->tms_utime );
                    ptms->tms_stime = flip_endian32( ptms->tms_stime );
#endif
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
#if defined( M68 ) && !defined( M68K )
                ptms->tms_utime = flip_endian32( ptms->tms_utime );
                ptms->tms_stime = flip_endian32( ptms->tms_stime );
                ptms->tms_cutime = flip_endian32( ptms->tms_cutime );
                ptms->tms_cstime = flip_endian32( ptms->tms_cstime );
#endif
#endif
            }

            // ticks is generally in hundredths of a second per sysconf( _SC_CLK_TCK )
            REG_TYPE sc_clk_tck = (REG_TYPE) 100;
#if defined( _WIN32 )
            REG_TYPE ticks = (REG_TYPE) GetTickCount64();
            ticks /= 10; // milliseconds to hundredths of a second
#else
            sc_clk_tck = sysconf( _SC_CLK_TCK );
            REG_TYPE ticks = times( 0 );
#endif
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
            ACCESS_REG( REG_RESULT ) = getpid();
            break;
        }
        case SYS_gettid:
        {
            ACCESS_REG( REG_RESULT ) = 1;
            break;
        }
        case SYS_renameat:
        case SYS_renameat2:
        {
            int olddirfd = (int) ACCESS_REG( REG_ARG0 );
            const char * oldpath = (const char *) cpu.getmem( ACCESS_REG( REG_ARG1 ) );
            int newdirfd = (int) ACCESS_REG( REG_ARG2 );
            const char * newpath = (const char *) cpu.getmem( ACCESS_REG( REG_ARG3 ) );
            unsigned int flags = ( SYS_renameat2 == ACCESS_REG( REG_SYSCALL ) ) ? (unsigned int) ACCESS_REG( REG_ARG4 ) : 0;
            tracer.Trace( "  renaming '%s' to '%s'\n", oldpath, newpath );
#if defined( _WIN32 ) || defined( M68K )
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
            void * buf = cpu.getmem( ACCESS_REG( REG_ARG0 ) );
            REG_TYPE buflen = ACCESS_REG( REG_ARG1 );
            unsigned int flags = (unsigned int) ACCESS_REG( REG_ARG2 );
            SIGNED_REG_TYPE result = 0;

#if defined(_WIN32) || defined(__APPLE__) || defined( M68K )
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
            ACCESS_REG( REG_RESULT ) = 0;
            break;
        }
        case SYS_pselect6:
        {
            int nfds = (int) ACCESS_REG( REG_ARG0 );
            uint64_t readfds = ACCESS_REG( REG_ARG1 );

            if ( 1 == nfds && 0 != readfds )
            {
                // check to see if stdin has a keystroke available

                ACCESS_REG( REG_RESULT ) = g_consoleConfig.portable_kbhit();
                tracer.Trace( "  pselect6 keystroke available on stdin: %llx\n", ACCESS_REG( REG_RESULT ) );
            }
            else
            {
                // lie and say no I/O is ready

                ACCESS_REG( REG_RESULT ) = 0;
            }

            break;
        }
        case SYS_ppoll_time32:
        {
            struct pollfd_syscall * pfds = (struct pollfd_syscall *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
            int nfds = (int) ACCESS_REG( REG_ARG1 );
            struct timespec_syscall * pts = (struct timespec_syscall *) cpu.getmem( ACCESS_REG( REG_ARG2 ) );
            int * psigmask = (int *) ( ( 0 == ACCESS_REG( REG_ARG3 ) ) ? 0 : cpu.getmem( ACCESS_REG( REG_ARG3 ) ) );

            tracer.Trace( "  count of file descriptors: %d\n", nfds );
            for ( int i = 0; i < nfds; i++ )
                tracer.Trace( "    fd %d: %d\n", i, pfds[ i ].fd );

            // lie and say no I/O is ready
            ACCESS_REG( REG_RESULT ) = 0;
            break;
        }
        case SYS_readlinkat:
        {
            int dirfd = (int) ACCESS_REG( REG_ARG0 );
            const char * pathname = (const char *) cpu.getmem( ACCESS_REG( REG_ARG1 ) );
            char * buf = (char *) cpu.getmem( ACCESS_REG( REG_ARG2 ) );
            size_t bufsiz = (size_t) ACCESS_REG( REG_ARG3 );
            tracer.Trace( "  readlinkat pathname %p == '%s', buf %p, bufsiz %zd, dirfd %d\n", pathname, pathname, buf, bufsiz, dirfd );
            int result = -1;

#if defined( _WIN32 ) || defined( M68K )
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
            int fd = (int) ACCESS_REG( REG_ARG0 );
            unsigned long request = (unsigned long) ACCESS_REG( REG_ARG1 );
            tracer.Trace( "  ioctl fd %d, request %lx\n", fd, request );
            struct local_kernel_termios * pt = (struct local_kernel_termios *) cpu.getmem( ACCESS_REG( REG_ARG2 ) );

            if ( 0 == fd || 1 == fd || 2 == fd ) // stdin, stdout, stderr
            {
#if defined( _WIN32 ) || defined( M68K )
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
#endif // _WIN32 || M68K
            }
            else if ( 1 == fd ) // stdout
            {
                if ( 0x5401 == request ) // TCGETS
                {
#if defined( _WIN32 ) || defined( M68K )
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
#endif // _WIN32 || M68K
                }

            }

            ACCESS_REG( REG_RESULT ) = 0;
            break;
        }
        case SYS_set_tid_address:
        {
            ACCESS_REG( REG_RESULT ) = 1;
            break;
        }
        case SYS_madvise:
        {
            ACCESS_REG( REG_RESULT ) = 0; // report success
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
            const char * pathname = (const char *) cpu.getmem( ACCESS_REG( REG_ARG1 ) );
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
            update_result_errno( cpu, 0x5549 ); // IU. Love wins all.
            break;
        }
        default:
        {
#ifdef M68
            printf( "error; ecall invoked with unknown command %u = %#x, a0 %#x, a1 %#x, a2 %#x\n",
                    ACCESS_REG( REG_SYSCALL ), ACCESS_REG( REG_SYSCALL ), ACCESS_REG( REG_ARG0 ), ACCESS_REG( REG_ARG1 ), ACCESS_REG( REG_ARG2 ) );
            tracer.Trace( "error; ecall invoked with unknown command %u = %#x, a0 %#x, a1 %#x, a2 %#x\n",
                          ACCESS_REG( REG_SYSCALL ), ACCESS_REG( REG_SYSCALL ), ACCESS_REG( REG_ARG0 ), ACCESS_REG( REG_ARG1 ), ACCESS_REG( REG_ARG2 ) );
#else
            printf( "error; ecall invoked with unknown command %llu = %llx, a0 %#llx, a1 %#llx, a2 %#llx\n",
                    ACCESS_REG( REG_SYSCALL ), ACCESS_REG( REG_SYSCALL ), ACCESS_REG( REG_ARG0 ), ACCESS_REG( REG_ARG1 ), ACCESS_REG( REG_ARG2 ) );
            tracer.Trace( "error; ecall invoked with unknown command %llu = %llx, a0 %#llx, a1 %#llx, a2 %#llx\n",
                          ACCESS_REG( REG_SYSCALL ), ACCESS_REG( REG_SYSCALL ), ACCESS_REG( REG_ARG0 ), ACCESS_REG( REG_ARG1 ), ACCESS_REG( REG_ARG2 ) );
#endif
            fflush( stdout );
            //ACCESS_REG( REG_RESULT ] = -1;
        }
    }
} //emulator_invoke_svc

#ifdef M68

// called when trap #15 is invoked for an IDE68K emulator system call
// hard-code register values because they differ from linux syscall calling convention

void emulator_invoke_68k_trap15( m68000 & cpu )
{
    uint16_t svc = cpu.getui16( cpu.pc + 2 ); // two bytes that are two bytes past the trap instruction
    tracer.Trace( "68k trap 16: svc %u, reg0 %x\n", svc, ACCESS_REG( 0 ) );

    switch( svc )
    {
        case 0: // exit
        {
            g_terminate = true;
            cpu.end_emulation();
            g_exit_code = (int) ACCESS_REG( 0 );
            tracer.Trace( "  emulated app exit code %d\n", g_exit_code );
            break;
        }
        case 1: // putch
        {
            uint8_t val = (uint8_t) ACCESS_REG( 0 );
            if ( 0xd != val )
            {
                size_t written = write( 1, &val, 1 );
                update_result_errno( cpu, (REG_TYPE) written );
            }
            break;
        }
        default:
        {
            tracer.Trace( "unimplemented m68k trap #15 service %u\n", svc );
            break;
        }
    }
} //emulator_invoke_68k_trap15

#endif

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

    printf( "hard termination!!!\n" );

    tracer.Trace( "%s (%s) fatal error: %s %0llx\n", APP_NAME, target_platform(), pcerr, error_value );
    printf( "%s (%s) fatal error: %s %0llx\n", APP_NAME, target_platform(), pcerr, error_value );

    uint64_t offset = 0;
#ifdef M68
    uint32_t offset32 = 0;
    const char * psymbol = emulator_symbol_lookup( cpu.pc, offset32 );
    offset = offset32;
#else
    const char * psymbol = emulator_symbol_lookup( cpu.pc, offset );
#endif

    if ( psymbol[ 0 ] )
    {
        tracer.Trace( "pc: %llx %s + %llx\n", cpu.pc, psymbol, offset );
        printf( "pc: %llx %s + %llx\n", (uint64_t) cpu.pc, psymbol, offset );
    }
    else
    {
        tracer.Trace( "pc: %llx\n", cpu.pc );
        printf( "pc: %llx\n", (uint64_t) cpu.pc );
    }

    tracer.Trace( "address space %llx to %llx\n", (uint64_t) g_base_address, (uint64_t) g_base_address + memory.size() );
    printf( "address space %llx to %llx\n", (uint64_t) g_base_address, (uint64_t) g_base_address + memory.size() );

    tracer.Trace( "  " );
    printf( "  " );

#ifdef M68

    for ( size_t i = 0; i < 8; i++ )
    {
        tracer.Trace( "d%zu: %8x, ", i, cpu.dregs[ i ].l );
        printf( "d%zu: %8x, ", i, cpu.dregs[ i ].l );

        if ( 3 == ( i & 3 ) )
        {
            tracer.Trace( "\n" );
            printf( "\n" );
            if ( 7 != i )
            {
                tracer.Trace( "  " );
                printf( "  " );
            }
        }
    }

    tracer.Trace( "  " );
    printf( "  " );

    for ( size_t i = 0; i < 8; i++ )
    {
        tracer.Trace( "a%zu: %8x, ", i, cpu.aregs[ i ] );
        printf( "a%zu: %8x, ", i, cpu.aregs[ i ] );

        if ( 3 == ( i & 3 ) )
        {
            tracer.Trace( "\n" );
            printf( "\n" );
            if ( 7 != i )
            {
                tracer.Trace( "  " );
                printf( "  " );
            }
        }
    }


#else

    for ( size_t i = 0; i < 32; i++ )
    {
#ifdef ARMOS
        tracer.Trace( "%02zu: %16llx, ", i, ACCESS_REG( i ) );
        printf( "%02zu: %16llx, ", i, ACCESS_REG( i ) );
#elif defined( RVOS )
        tracer.Trace( "%4s: %16llx, ", riscv_register_names[ i ], ACCESS_REG( i ) );
        printf( "%4s: %16llx, ", riscv_register_names[ i ], ACCESS_REG( i ) );
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

#endif // M68

    tracer.Trace( "%s\n", build_string() );
    printf( "%s\n", build_string() );

    tracer.Flush();
    fflush( stdout );
    exit( 1 );
} //emulator_hard_termination

#ifdef M68

const char * bdos_functions[] =
{
    "system reset",                        // 0
    "console input",
    "console output",
    "auxiliary input",
    "auxiliary output",
    "list output",
    "direct console i/o",
    "get i/o byte",
    "set i/o byte",
    "print string",
    "read console buffer",                 // 10
    "get console status",
    "return version number",
    "reset disk system",
    "select disk",
    "open file",
    "close file",
    "search for first",
    "search for next",
    "delete file",
    "read sequential",                     // 20
    "write sequential",
    "make file",
    "rename file",
    "return login vector",
    "return current disk",
    "set dma address",
    "27 is unused",
    "write protect disk",
    "get read-only vector",
    "set file attributes",                 // 30
    "get disk parmameters",
    "get/set user code",
    "read random",
    "write random",
    "compute file size",
    "set random record",
    "reset drive",
    "38 is unused",
    "39 is unused",
    "write random with zero fill",         // 40
    "41 is unused",
    "42 is unused",
    "43 is unused",
    "44 is unused",
    "45 is unused",
    "get disk free space",
    "chain to program",
    "flush buffers",
    "49 is unused",
    "direct BIOS call",                    // 50
    "51 is unused",
    "52 is unused",
    "53 is unused",
    "54 is unused",
    "55 is unused",
    "56 is unused",
    "57 is unused",
    "58 is unused",
    "program load",
    "60 is unused",                        // 60
    "set exception vector",
    "set supervisor state",
    "get/set tpa limits"
};

const char * bdos_function( uint32_t id )
{
    if ( id < _countof( bdos_functions ) )
        return bdos_functions[ id ];

    return "unknown";
};

void append_string( char * pc, const char * a )
{
    size_t len = strlen( pc );
    if ( 0 != len )
    {
        pc[ len++ ] = ',';
        pc[ len++ ] = ' ';
    }

    strcpy( pc + len, a );
} //append_string

#pragma pack( push, 1 )
struct SymbolEntryCPM
{
    char name[ 8 ];
    uint16_t type;
    uint32_t value;

    void swap_endianness()
    {
        type = swap_endian16( type );
        value = swap_endian32( value );
    }

    const char * get_type()
    {
        static char ac[ 80 ];
        ac[ 0 ] = 0;

        if ( 0x8000 & type )
            append_string( ac, "defined" );
        if ( 0x4000 & type )
            append_string( ac, "equated" );
        if ( 0x2000 & type )
            append_string( ac, "global" );
        if ( 0x1000 & type )
            append_string( ac, "equated-register" );
        if ( 0x800 & type )
            append_string( ac, "external reference" );
        if ( 0x400 & type )
            append_string( ac, "data-based-relocatable" );
        if ( 0x200 & type )
            append_string( ac, "text-based-relocatable" );
        if ( 0x100 & type )
            append_string( ac, "bss-based-relocatable" );

        return ac;
    }

    void trace()
    {
        tracer.Trace( "  %#16x", value );
        tracer.Trace( "  %10s", name );
        tracer.Trace( "  %s\n", get_type() );
    }
};
#pragma pack(pop)

static vector<SymbolEntryCPM> g_cpmSymbols;

static int symbol_find_compare32( const void * a, const void * b )
{
    ElfSymbol32 & sa = * (ElfSymbol32 *) a;
    ElfSymbol32 & sb = * (ElfSymbol32 *) b;

    if ( 0 == sa.size ) // a is the key
    {
        if ( ( sa.value == sb.value ) || ( sa.value > sb.value && sa.value < ( sb.value + sb.size ) ) )
            return 0;
    }
    else // b is the key
    {
        if ( ( sb.value == sa.value ) || ( sb.value > sa.value && sb.value < ( sa.value + sa.size ) ) )
            return 0;
    }

    if ( sa.value > sb.value )
        return 1;
    return -1;
} //symbol_find_compare32

static int symbol_find_compare_cpm( const void * a, const void * b )
{
    SymbolEntryCPM & sa = * (SymbolEntryCPM *) a;
    SymbolEntryCPM & sb = * (SymbolEntryCPM *) b;

    if ( 0 == sa.name[0] ) // a is the key
    {
        SymbolEntryCPM * pnext = ( ( & sb ) + 1 );
        if ( ( sa.value == sb.value ) || ( sa.value > sb.value && sa.value < pnext->value ) )
            return 0;
    }
    else // b is the key
    {
        SymbolEntryCPM * pnext = ( ( & sa ) + 1 );
        if ( ( sb.value == sa.value ) || ( sb.value > sa.value && sb.value < pnext->value ) )
            return 0;
    }

    if ( sa.value > sb.value )
        return 1;
    return -1;
} //symbol_find_compare_cpm

#else

static int symbol_find_compare( const void * a, const void * b )
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

#endif

vector<char> g_string_table;      // strings in the elf image
vector<ElfSymbol64> g_symbols;    // symbols in the elf image
vector<ElfSymbol32> g_symbols32;  // symbols in the elf image

// returns the best guess for a symbol name for the address

#ifdef M68

const char * emulator_symbol_lookup( uint32_t address, uint32_t & offset )
{
    if ( address < g_base_address || address > ( g_base_address + memory.size() ) )
        return "";

    // if no elf symbols, try CPM symbols

    if ( 0 == g_symbols32.size() )
    {
        if ( 0 != g_cpmSymbols.size() )
        {
            SymbolEntryCPM key = {0};
            key.value = address;
            SymbolEntryCPM * psym = (SymbolEntryCPM *) my_bsearch( &key, g_cpmSymbols.data(), g_cpmSymbols.size(), sizeof( key ), symbol_find_compare_cpm );
            if ( 0 != psym )
            {
                offset = address - psym->value;
                return psym->name;
            }
        }

        return "";
    }

    ElfSymbol32 key = {0};
    key.value = address;

    ElfSymbol32 * psym = (ElfSymbol32 *) my_bsearch( &key, g_symbols32.data(), g_symbols32.size(), sizeof( key ), symbol_find_compare32 );

    if ( 0 != psym )
    {
        offset = address - psym->value;
        return & g_string_table[ psym->name ];
    }

    offset = 0;
    return "";
} //emulator_symbol_lookup

static int symbol_compare32( const void * a, const void * b )
{
    ElfSymbol32 * pa = (ElfSymbol32 *) a;
    ElfSymbol32 * pb = (ElfSymbol32 *) b;

    if ( pa->value > pb->value )
        return 1;
    if ( pa->value == pb->value )
        return 0;
    return -1;
} //symbol_compare32

#else

const char * emulator_symbol_lookup( uint64_t address, uint64_t & offset )
{
    if ( address < g_base_address || address > ( g_base_address + memory.size() ) )
        return "";

    ElfSymbol64 key = {0};
    key.value = address;

    ElfSymbol64 * psym = (ElfSymbol64 *) my_bsearch( &key, g_symbols.data(), g_symbols.size(), sizeof( key ), symbol_find_compare );

    if ( 0 != psym )
    {
        offset = address - psym->value;
        return & g_string_table[ psym->name ];
    }

    offset = 0;
    return "";
} //emulator_symbol_lookup

static int symbol_compare( const void * a, const void * b )
{
    ElfSymbol64 * pa = (ElfSymbol64 *) a;
    ElfSymbol64 * pb = (ElfSymbol64 *) b;

    if ( pa->value > pb->value )
        return 1;
    if ( pa->value == pb->value )
        return 0;
    return -1;
} //symbol_compare

#endif

static void remove_spaces( char * p )
{
    char * o;
    for ( o = p; *p; p++ )
    {
        if ( ' ' != *p )
            *o++ = *p;
    }
    *o = 0;
} //remove_spaces

static const char * image_type( uint16_t e_type )
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

static int ends_with( const char * str, const char * end )
{
    size_t len = strlen( str );
    size_t lenend = strlen( end );

    if ( len < lenend )
        return false;

    return ( 0 == _stricmp( str + len - lenend, end ) );
} //ends_with

#ifdef M68

#define CPM_FILENAME_LEN ( 8 + 3 + 1 + 1 ) // name + type + dot + null

// this struct is used to cache FILE * objects to avoid open/close for each file access

struct FileEntry
{
    char acName[ CPM_FILENAME_LEN ];
    FILE * fp;
};

static bool g_forceLowercase = false;
static uint8_t * g_DMA = 0;
static vector<FileEntry> g_fileEntries;

FILE * RemoveFileEntry( char * name )
{
    for ( size_t i = 0; i < g_fileEntries.size(); i++ )
    {
        if ( !strcmp( name, g_fileEntries[ i ].acName ) )
        {
            FILE * fp = g_fileEntries[ i ].fp;
            tracer.Trace( "  removing file entry '%s'\n", name );
            g_fileEntries.erase( g_fileEntries.begin() + i );
            return fp;
        }
    }

    tracer.Trace( "ERROR: could not remove file entry for '%s'\n", name );
    return 0;
} //RemoveFileEntry

FILE * FindFileEntry( char * name )
{
    for ( size_t i = 0; i < g_fileEntries.size(); i++ )
    {
        if ( !strcmp( name, g_fileEntries[ i ].acName ) )
        {
            tracer.Trace( "  found file entry '%s'\n", name );
            return g_fileEntries[ i ].fp;
        }
    }

    tracer.Trace( "  could not find an open file entry for '%s'; that might be OK\n", name );
    return 0;
} //FindFileEntry

#pragma pack( push, 1 )

struct ExceptionParameterBlockCPM
{
    uint16_t vector;
    uint32_t newValue;
    uint32_t oldValue;

    void swap_endianness()
    {
        vector = swap_endian16( vector );
        newValue = swap_endian32( newValue );
        oldValue = swap_endian32( oldValue );
    }

    void trace()
    {
        tracer.Trace( "  ExceptionParameterBlockCPM:\n" );
        tracer.Trace( "    vector %#x\n", vector );
        tracer.Trace( "    newValue %#x\n", newValue );
        tracer.Trace( "    oldValue %#x\n", oldValue );
    }
};

struct LoadParameterBlockCPM
{
    uint32_t fcbOfChildApp;      // the calling app must have opened the file that's here
    uint32_t lowestAddress;      // the calling app specifies this
    uint32_t highestAddress;     // the calling app specifies this
    uint32_t childBasePage;      // returned by bdos
    uint32_t childStackPointer;  // returned by bdos
    uint16_t loaderControlFlags; // 0 == load in lowest possible address. 1 == load in highest possible address

    void swap_endianness()
    {
         fcbOfChildApp = swap_endian32( fcbOfChildApp );
         lowestAddress = swap_endian32( lowestAddress );
         highestAddress = swap_endian32( highestAddress );
         childBasePage = swap_endian32( childBasePage );
         childStackPointer = swap_endian32( childStackPointer );
         loaderControlFlags= swap_endian16( loaderControlFlags );
    }

    void trace()
    {
        tracer.Trace( "  load parameter block:\n" );
        tracer.Trace( "    fcb of child app:               %lx\n", fcbOfChildApp );
        tracer.Trace( "    lowest address of child app:    %lx\n", lowestAddress );
        tracer.Trace( "    highest address of child app:   %lx\n", highestAddress );
        tracer.Trace( "    child base page from bdos:      %lx\n", childBasePage );
        tracer.Trace( "    child stack pointer from bdos:  %lx\n", childStackPointer );
        tracer.Trace( "    loader control flags:           %x\n", loaderControlFlags );
    }
};

struct HeaderCPM68K    // .68k executable files for cp/m 68k v1.3
{
    uint16_t signature;
    uint32_t cb_text;
    uint32_t cb_data;
    uint32_t cb_bss;
    uint32_t cb_symbols;
    uint32_t reserved;
    uint32_t text_start;
    uint16_t relocation_flag; // two bytes, but only the low byte has the flag

    void swap_endianness()
    {
        signature = swap_endian16( signature );
        cb_text = swap_endian32( cb_text );
        cb_data = swap_endian32( cb_data );
        cb_bss = swap_endian32( cb_bss );
        cb_symbols = swap_endian32( cb_symbols );
        reserved = swap_endian32( reserved );
        text_start = swap_endian32( text_start );
        relocation_flag = swap_endian16( relocation_flag );
    }

    void trace()
    {
        tracer.Trace( "cpm68k executable header:\n" );
        tracer.Trace( "  signature: %#x\n", signature );
        tracer.Trace( "  cb_text: %#x\n", cb_text );
        tracer.Trace( "  cb_data: %#x\n", cb_data );
        tracer.Trace( "  cb_bss: %#x\n", cb_bss );
        tracer.Trace( "  cb_symbols: %#x\n", cb_symbols );
        tracer.Trace( "  text_start: %#x\n", text_start );
        tracer.Trace( "  relocation_flag: %#x\n", relocation_flag );
    }
};

struct FCBCPM68K // file control block for cp/m
{
    uint8_t dr;         // drive
    char f[ 8 ];        // filename
    char t[ 3 ];        // file type
    uint8_t ex;         // current extent number. normally set to 0 by user but is in range 0-31
    uint8_t s1;
    uint8_t s2;         // reserved for system use. set to 0 for open/make/search
    uint8_t rc;         // record count, reserved for system use
    uint8_t d[ 16 ];
    uint8_t cr;         // current reacord to be read or written for sequential read/write operations. apps must set approprately.
    uint8_t r0;         // (optional if app is doing random I/O) random record number. most significant byte in r0 then r1 then r2 as a 24-bit record
    uint8_t r1;         // the byte order is the opposite of CP/M 2.2 for 8080 plus r2 is actually used!
    uint8_t r2;

    void make_filename( char * p )
    {
        for ( int i = 0; i < 8; i++ )
        {
            if ( ' ' == ( 0x7f & f[ i ] ) )
                break;
            *p++ = ( 0x7f & f[ i ] );
        }
        if ( ' ' != t[ 0 ] )
        {
            *p++ = '.';
            for ( int i = 0; i < 3; i++ )
            {
                if ( ' ' == ( 0x7f & t[ i ] ) )
                    break;
                *p++ = ( 0x7f & t[ i ] );
            }
        }
        *p++ = 0;
    }
    // r0 and r1 are a 16-bit count of 128 byte records

    uint32_t GetRandomIOOffset() { return ( (uint32_t) this->r0 << 16 ) | ( (uint32_t) this->r1 << 8 ) | this->r2; }

    void SetRandomIOOffset( uint32_t o )
    {
        this->r2 = ( 0xff & o );
        this->r1 = ( 0xff & ( o >> 8 ) );
        this->r0 = ( 0xff & ( o >> 16 ) );
    } //SetRandomIOOffset

    void SetRecordCount( FILE * fp )
    {
        // set rc to file size in 128 byte records if < 16k, else 128
        uint32_t file_size = portable_filelen( fp );
        if ( file_size >= ( 16 * 1024 ) ) // CP/M 2.2 does this and Whitesmith C's A80.COM and LNK.COM depend on it
            this->rc = 128;
        else
        {
            uint32_t tail_size = ( file_size % ( 16 * 1024 ) ); // won't matter because of 16k check above
            this->rc = (uint8_t) ( tail_size / 128 );
            if ( 0 != ( tail_size % 128 ) )
                this->rc++;
        }
    } //SetRecordCount

    void UpdateSequentialOffset( uint32_t offset )
    {
        cr = (uint8_t) ( ( offset % ( (uint32_t) 16 * 1024 ) ) / (uint32_t) 128 );
        ex = (uint8_t) ( ( offset % ( (uint32_t) 512 * 1024 ) ) / ( (uint32_t) 16 * 1024 ) );
        s2 = (uint8_t) ( offset / ( (uint32_t) 512 * 1024 ) );
        #ifdef WATCOM
        tracer.Trace( "  new offset: %u, s2 %u, ex %u, cr %u\n", (uint16_t) offset, (uint16_t) s2, (uint16_t) ex, (uint16_t) cr );
        #else
        tracer.Trace( "  new offset: %u, s2 %u, ex %u, cr %u\n", offset, s2, ex, cr );
        #endif
    } //UpdateSequentialOffset

    uint32_t GetSequentialOffset()
    {
        uint32_t curr = (uint32_t) cr * 128;
        curr += ( (uint32_t) ex * ( (uint32_t) 16 * 1024 ) );
        curr += ( (uint32_t) s2 * ( (uint32_t) 512 * 1024 ) );
        return curr;
    } //GetSequentialOffset

    void Trace( bool justArg = false ) // justArg is the first 16 bytes at app startup
    {
        tracer.Trace( "  FCB at address %04x:\n", (uint32_t) ( (uint8_t * ) this - memory.data() ) );
        tracer.Trace( "    drive:    %#x == %c\n", dr, ( 0 == dr ) ? 'A' : 'A' + dr - 1 );
        tracer.Trace( "    filename: '%c%c%c%c%c%c%c%c'\n", 0x7f & f[0], 0x7f & f[1], 0x7f & f[2], 0x7f & f[3],
                                                            0x7f & f[4], 0x7f & f[5], 0x7f & f[6], 0x7f & f[7] );
        tracer.Trace( "    filetype: '%c%c%c'\n", 0x7f & t[0], 0x7f & t[1], 0x7f & t[2] );
        tracer.Trace( "    R S A:    %d %d %d\n", 0 != ( 0x80 & t[0] ), 0 != ( 0x80 & t[1] ), 0 != ( 0x80 & t[2] ) );
        tracer.Trace( "    ex:       %d\n", ex );
        tracer.Trace( "    s1:       %u\n", s1 );
        tracer.Trace( "    s2:       %u\n", s2 );
        tracer.Trace( "    rc:       %u\n", rc );
        if ( !justArg )
        {
            tracer.Trace( "    cr:       %u\n", cr );
            tracer.Trace( "    r0:       %u\n", r0 );
            tracer.Trace( "    r1:       %u\n", r1 );
            tracer.Trace( "    r2:       %u\n", r2 );
        }
    } //Trace
};

struct BasePageCPM // base page for cp/m -- the first 256 bytes in memory
{
    uint32_t lowest_tpa;                // 0
    uint32_t highest_tpa;               // 4
    uint32_t start_text;                // 8
    uint32_t cb_text;                   // c
    uint32_t start_data;                // 10
    uint32_t cb_data;                   // 14
    uint32_t start_bss;                 // 18
    uint32_t cb_bss;                    // 1c
    uint32_t cb_after_bss;              // 20
    uint8_t drive;                      // 24 where the app was loaded
    uint8_t reserved[ 19 ];             // 25 reserved. used for exit trap when an app returns from _start
    FCBCPM68K secondFCB;                // 38
    FCBCPM68K firstFCB;                 // 5c
    uint8_t cb_command_tail;            // 80 also start of default DMA buffer
    uint8_t command_tail[ 127 ];        // 81
};

#pragma pack(pop)

static int symbol_compare_cpm( const void * a, const void * b )
{
    SymbolEntryCPM * pa = (SymbolEntryCPM *) a;
    SymbolEntryCPM * pb = (SymbolEntryCPM *) b;

    if ( pa->value > pb->value )
        return 1;
    if ( pa->value == pb->value )
        return 0;
    return -1;
} //symbol_compare_cpm

bool write_fcb_arg( FCBCPM68K * arg, char * pc )
{
    if ( ':' == pc[ 1 ] )
    {
        if ( pc[0] > 'P' || pc[0] < 'A' )
            return false; // don't write arguments that don't look like filenames

        arg->dr = 1 + pc[0] - 'A';
        pc += 2;
    }

    char * dot = strchr( pc, '.' );
    if ( dot )
    {
        memcpy( & ( arg->f ), pc, get_min( (size_t) 8, (size_t) ( dot - pc ) ) );
        memcpy( & ( arg->t ), dot + 1, get_min( (size_t) 3, strlen( dot + 1 ) ) );
    }
    else
        memcpy( & ( arg->f ), pc, get_min( (size_t) 8, strlen( pc ) ) );

    return true;
} //write_fcb_arg

char get_next_kbd_char()
{
    return (char) ConsoleConfiguration::portable_getch();
} //get_next_kbd_char

bool is_kbd_char_available()
{
    return g_consoleConfig.portable_kbhit();
} //is_kbd_char_available

bool cpm_read_console( char * buf, size_t bufsize, uint8_t & out_len )
{
    char ch = 0;
    out_len = 0;
    while ( out_len < (uint8_t) bufsize )
    {
        ch = get_next_kbd_char();
        tracer.Trace( "  get_next_kbd_char read character %02x -- '%c'\n", ch, printable( ch ) );

        // CP/M read console buffer treats these control characters as special: c, e, h, j, m, r, u, x
        // per http://www.gaby.de/cpm/manuals/archive/cpm22htm/ch5.htm
        // Only c, h, j, and m are currently handled correctly.
        // ^c means exit the currently running app in CP/M if it's the first character in the buffer

        if ( ( 3 == ch ) && ( 0 == out_len ) )
            return true;

        if ( '\n' == ch || '\r' == ch )
            break;

        if ( 0x7f == ch || 8 == ch ) // backspace (it's not 8 for some reason)
        {
            if ( out_len > 0 )
            {
                printf( "\x8 \x8" );
                fflush( stdout );
                out_len--;
            }
        }
        else
        {
            printf( "%c", ch );
            fflush( stdout );
            buf[ out_len++ ] = ch;
        }
    }

    return false;
} //cpm_read_console

bool read_symbols_cpm( FILE * fp, HeaderCPM68K & head, uint32_t text_base, uint32_t data_base, uint32_t bss_base )
{
    if ( 0 != head.cb_symbols ) // if they exist, load symbols so disassembly and hard termination can show them
    {
        uint32_t symbol_count = head.cb_symbols / sizeof( SymbolEntryCPM );
        g_cpmSymbols.resize( symbol_count );
        fseek( fp, (long) sizeof( head ) + head.cb_text + head.cb_data, SEEK_SET );
        size_t read = fread( g_cpmSymbols.data(), head.cb_symbols, 1, fp );
        if ( 1 != read )
        {
            printf( "can't read symbol data of cp/m 68k image file\n" );
            return false;
        }

        for ( uint32_t i = 0; i < symbol_count; i++ )
        {
            SymbolEntryCPM & sym = g_cpmSymbols[ i ];
            sym.swap_endianness();
            if ( sym.type & 0x400 )
                sym.value += data_base;
            else if ( sym.type & 0x200 )
                sym.value += text_base;
            else if ( sym.type & 0x100 )
                sym.value += bss_base;

            // the DR tools create some symbols that aren't null-terminated
            sym.name[ 7 ] = 0;
        }

        // add a final entry with a very large value so the lookup doesn't have to worry about that edge case

        SymbolEntryCPM last;
        strcpy( last.name, "!last" );
        last.value = 0xffffffff;
        g_cpmSymbols.push_back( last );

        my_qsort( g_cpmSymbols.data(), g_cpmSymbols.size(), sizeof( SymbolEntryCPM ), symbol_compare_cpm );

        tracer.Trace( "symbols:\n" );
        for ( uint32_t i = 0; i < symbol_count; i++ )
            g_cpmSymbols[ i ].trace();
    }

    return true;
} //read_symbols_cpm

bool handle_relocations_cpm( FILE * fp, HeaderCPM68K & head, uint32_t text_base, uint32_t data_base, uint32_t bss_base )
{
    if ( 0 == head.relocation_flag ) // 0 means they exist
    {
        uint16_t * pimage = (uint16_t *) ( memory.data() + text_base );
        uint32_t relocation_words = ( head.cb_text + head.cb_data ) / 2;
        vector<uint16_t> relocations;
        relocations.resize( relocation_words );
        fseek( fp, (long) sizeof( head ) + head.cb_text + head.cb_data + head.cb_symbols, SEEK_SET );
        size_t read = fread( relocations.data(), relocation_words * 2, 1, fp );
        if ( 1 != read )
        {
            printf( "can't read relocations data of cp/m 68k image file\n" );
            return false;
        }

        bool longword_mode = false;

        for ( uint32_t i = 0; i < relocation_words; i++ )
        {
            uint16_t r = 7 & swap_endian16( relocations[ i ] );

            if ( 1 == r )
            {
                //tracer.Trace( "updating relocation offset %u with data_base %#x, longword_mode %u\n", i, data_base, longword_mode );
                if ( longword_mode )
                {
                    uint32_t * pfull = (uint32_t *) ( pimage + i - 1 );
                    *pfull = swap_endian32( data_base + swap_endian32( *pfull ) );
                    longword_mode = false;
                }
                else
                    pimage[ i ] = swap_endian16( ( 0xffff & data_base ) + swap_endian16( pimage[ i ] ) );
            }
            else if ( 2 == r )
            {
                //tracer.Trace( "updating relocation offset %u with text_base %#x, longword_mode %u\n", i, text_base, longword_mode );
                if ( longword_mode )
                {
                    uint32_t * pfull = (uint32_t *) ( pimage + i - 1 );
                    *pfull = swap_endian32( text_base + swap_endian32( *pfull ) );
                    longword_mode = false;
                }
                else
                    pimage[ i ] = swap_endian16( (uint16_t) text_base + swap_endian16( pimage[ i ] ) );
            }
            else if ( 3 == r )
            {
                //tracer.Trace( "updating relocation offset %u with bss_base %#x, longword_mode %u\n", i, bss_base, longword_mode );
                if ( longword_mode )
                {
                    uint32_t * pfull = (uint32_t *) ( pimage + i - 1 );
                    *pfull = swap_endian32( bss_base + swap_endian32( *pfull ) );
                    longword_mode = false;
                }
                else
                    pimage[ i ] = swap_endian16( (uint16_t) bss_base + swap_endian16( pimage[ i ] ) );
            }
            else if ( 5 == r )
                longword_mode = true;
            else
                longword_mode = false;
        }
    }

    return true;
} //handle_relocations_cpm

bool load59_cpm68k( FILE *fp, uint32_t lowestAddress, uint32_t highestAddress, uint16_t loaderControlFlags, uint32_t & basePage, uint32_t & stackPointer )
{
    if ( 0 != loaderControlFlags )
    {
        tracer.Trace( "ERROR: only loading to lowest address is implemented\n" );
        printf( "ERROR: only loading to lowest address is implemented\n" );
        return false;
    }

    g_cpmSymbols.resize( 0 ); // throw away parent symbols
    uint32_t child_memory_size = highestAddress + 1 - lowestAddress;

    fseek( fp, (long) 0, SEEK_SET );
    HeaderCPM68K head;
    size_t read = fread( &head, sizeof( head ), 1, fp );
    if ( 1 != read )
    {
        printf( "can't read header of cp/m 68k image file\n" );
        return false;
    }

    head.swap_endianness();
    head.trace();

    if ( 0x601a != head.signature )
    {
        printf( "header of cp/m 68k image file isn't standard 0x601a:\n" );
        return false;
    }

    if ( 0 == lowestAddress )    // ddt loads debugged apps at 0. Don't overwrite trap vectors; use a standard address
        lowestAddress = 0x7a00;

    uint32_t text_base;
    if ( 0 != head.relocation_flag ) // 0 means they exist, != 0 means they don't exist
        text_base = head.text_start;
    else
        text_base = lowestAddress + 0x100; // leave room for the base page

    uint32_t image_size = head.cb_text + head.cb_data + head.cb_bss;
    basePage = lowestAddress;
    stackPointer = highestAddress & 0xfffffffe; // make sure it's 2-byte aligned
    BasePageCPM * pbasepage = (BasePageCPM *) ( memory.data() + basePage );

    fseek( fp, (long) sizeof( head ), SEEK_SET );
    read = fread( memory.data() + text_base, head.cb_text + head.cb_data, 1, fp );
    if ( 1 != read )
    {
        printf( "can't read text and data segments of cp/m 68k image file\n" );
        return false;
    }

    // malloc / brk in the C runtime for DR C use some of these values

    pbasepage->lowest_tpa = swap_endian32( lowestAddress );
    pbasepage->highest_tpa = swap_endian32( highestAddress );
    pbasepage->start_text = swap_endian32( text_base );
    pbasepage->cb_text = swap_endian32( head.cb_text );
    pbasepage->start_data = swap_endian32( text_base + head.cb_text );
    pbasepage->cb_data = swap_endian32( head.cb_data );
    pbasepage->start_bss = swap_endian32( text_base + head.cb_text + head.cb_data );
    pbasepage->cb_bss = swap_endian32( head.cb_bss );
    pbasepage->cb_after_bss = swap_endian32( highestAddress + 1 - lowestAddress + 0x100 + head.cb_text + head.cb_data + head.cb_bss );

    uint32_t data_base = text_base; // + head.cb_text; with 0x601a all bases belong to text_base
    uint32_t bss_base = text_base; // data_base + head.cb_data;

    if ( !handle_relocations_cpm( fp, head, text_base, data_base, bss_base ) )
        return false;

    if ( !read_symbols_cpm( fp, head, text_base, data_base, bss_base ) )
        return false;

    tracer.Trace( "memory map from highest to lowest addresses:\n" );
    tracer.Trace( "  actual top of stack:                                %x\n", stackPointer );
    tracer.Trace( "  <stack>\n" );
    tracer.Trace( "  <unallocated space between brk and the stack>\n" );
    tracer.Trace( "  end_of_bss / current brk:                           %x\n", text_base + head.cb_text + head.cb_data  + head.cb_bss );
    tracer.Trace( "  <uninitialized bss data\n" );
    tracer.Trace( "  start of bss segment:                               %x\n", text_base + head.cb_text + head.cb_data );
    tracer.Trace( "  <initialized data from the .68k file>\n" );
    tracer.Trace( "  start of data segment:                              %x\n", text_base + head.cb_text );
    tracer.Trace( "  <code from the .68k file>\n" );
    tracer.Trace( "  initial pc execution_addess + start of code         %x\n", text_base );
    tracer.Trace( "  start of base page:                                 %x\n", basePage );
    tracer.Trace( "  start of the address space:                         %x\n", g_base_address );

    tracer.Trace( "first 512 bytes starting at base page:\n" );
    tracer.TraceBinaryData( memory.data() + basePage, 512, 8 );

    return true;
} //load59_cpm68k

bool load_cpm68k( const char * acApp, const char * acAppArgs )
{
    assert( 256 == sizeof( BasePageCPM ) ); // make sure the structure was defined correctly

    // if this is being called from the bdos chain call, reset global data structures.
    memory.resize( 0 );
    g_cpmSymbols.resize( 0 );

    FILE * fp = fopen( acApp, "rb" );
    if ( !fp )
    {
        printf( "can't open cp/m 68k image file: %s\n", acApp );
        return false;
    }

    CFile file( fp );
    HeaderCPM68K head;
    size_t read = fread( &head, sizeof( head ), 1, fp );
    if ( 1 != read )
    {
        printf( "can't read header of cp/m 68k image file: %s\n", acApp );
        return false;
    }

    head.swap_endianness();
    head.trace();

    if ( 0x601a != head.signature )
    {
        printf( "header of cp/m 68k image file isn't standard no-relocation 0x601a: %s\n", acApp );
        return false;
    }

    uint32_t text_base;
    if ( 0 != head.relocation_flag ) // 0 means they exist, != 0 means they don't exist
        text_base = head.text_start;
    else
        text_base = 0x7b00; // arbitrary, but this is what an actual cp/m 68k machine uses

    uint32_t image_size = head.cb_text + head.cb_data + head.cb_bss;
    uint32_t memory_size = 0x100 + text_base + image_size; // base page + offset from 0 to start of code + total size of image loaded from the executable

    // make it 4-byte aligned

    if ( memory_size & 3 )
    {
        memory_size += 4;
        memory_size &= ~3;
    }

    g_end_of_data = memory_size;
    g_brk_offset = memory_size;
    g_highwater_brk = memory_size;
    memory_size += g_brk_commit;

    g_bottom_of_stack = memory_size;
    memory_size += g_stack_commit;

    memory.resize( memory_size );
    memset( memory.data(), 0, memory.size() );

    // put the supervisor stack pointer in the first 4 bytes of RAM.
    * (uint32_t *) memory.data() = swap_endian32( 0x2000 ); // arbitrary, but above the vector table and below the typical cp/m 68k base page

    g_base_address = 0;
    uint32_t base_page = text_base - 0x100; // where the base page (256 bytes) resides
    BasePageCPM * pbasepage = (BasePageCPM *) ( memory.data() + base_page );
    g_execution_address = text_base;
    g_top_of_stack = (REG_TYPE) g_bottom_of_stack + g_stack_commit;
    pbasepage->reserved[ 1 ] = 0x22; // move.l d0, d1.  return code. at at offset 0x26 in the base page (not a cp/m standard)
    pbasepage->reserved[ 2 ] = 0x00;
    pbasepage->reserved[ 3 ] = 0x70; // moveq #93, d0   linux exit function
    pbasepage->reserved[ 4 ] = 0x5d;
    pbasepage->reserved[ 5 ] = 0x4e; // trap 0          invoke linux syscall to exit the app
    pbasepage->reserved[ 6 ] = 0x40;

    // per the cp/m 68k spec there must be two 32-bit values at the top of the stack for base address and return location at app completion
    g_top_of_stack -= 8;
    uint32_t * preturn_address = (uint32_t *) & memory[ g_top_of_stack ];
    uint32_t * pbase_page_address = (uint32_t *) & memory[ g_top_of_stack + 4 ];
    *preturn_address = swap_endian32( base_page + 0x26 );
    *pbase_page_address = swap_endian32( base_page );
    tracer.Trace( "memory at top of stack address %#x:\n", g_top_of_stack );
    tracer.TraceBinaryData( & memory[ g_top_of_stack ], 8, 4 );

    fseek( fp, (long) sizeof( head ), SEEK_SET );
    read = fread( memory.data() + text_base, head.cb_text + head.cb_data, 1, fp );
    if ( 1 != read )
    {
        printf( "can't read text and data segments of cp/m 68k image file: %s\n", acApp );
        return false;
    }

    while ( ' ' == *acAppArgs )
        acAppArgs++;

    uint8_t arg_len = (uint8_t) strlen( acAppArgs );
    if ( arg_len > 126 )
    {
        printf( "app arguments for cp/m can't be > 126 characters long\n" );
        return false;
    }

    memset( & ( pbasepage->firstFCB.f ), ' ', 11 ); // 8 filename + 3 type
    memset( & ( pbasepage->secondFCB.f ), ' ', 11 );

    pbasepage->cb_command_tail = arg_len;
    strcpy( (char *) ( pbasepage->command_tail ), acAppArgs );
    tracer.Trace( "arg_len %u, command tail %s\n", arg_len, acAppArgs );

    // put what looks like filenames in the command tail in the two FCBs in the base page. Apps don't much use this.

    char acCopy[ 128 ];
    strcpy( acCopy, acAppArgs );

    char * pcArg1 = 0;
    char * pcArg2 = 0;
    char * pa = acCopy;

    while ( *pa && !pcArg1 && !pcArg2 )
    {
        if ( ' ' == *pa )
            pa++;
        else if ( '-' == *pa )
        {
            while ( *pa && ' ' != *pa )
                pa++;
        }
        else if ( ':' == * ( pa + 1 ) )
            pa += 2;
        else if ( !pcArg1 )
        {
            pcArg1 = pa;
            while ( *pa && ' ' != *pa )
                pa++;
        }
        else
        {
            pcArg2 = pa;
            while ( *pa && ' ' != *pa )
                pa++;
        }
    }

    if ( pcArg1 )
    {
        char * p = pcArg1;
        while ( *p )
        {
            if ( ' ' == *p )
                *p = 0;
            else
                p++;
        }
        tracer.Trace( "    arg1: '%s'\n", pcArg1 );
        strupr( pcArg1 );
        write_fcb_arg( & ( pbasepage->firstFCB ), pcArg1 );
    }
    if ( pcArg2 )
    {
        char * p = pcArg2;
        while ( *p )
        {
            if ( ' ' == *p )
                *p = 0;
            else
                p++;
        }
        tracer.Trace( "    arg2: '%s'\n", pcArg2 );
        strupr( pcArg2 );
        write_fcb_arg( & ( pbasepage->secondFCB ), pcArg2 );
    }

    // malloc / brk in the C runtime for DR C use some of these values

    pbasepage->lowest_tpa = 0;
    pbasepage->highest_tpa = swap_endian32( g_base_address + memory_size - 1 );
    pbasepage->start_text = swap_endian32( text_base );
    pbasepage->cb_text = swap_endian32( head.cb_text );
    pbasepage->start_data = swap_endian32( text_base + head.cb_text );
    pbasepage->cb_data = swap_endian32( head.cb_data );
    pbasepage->start_bss = swap_endian32( text_base + head.cb_text + head.cb_data );
    pbasepage->cb_bss = swap_endian32( head.cb_bss );
    pbasepage->cb_after_bss = swap_endian32( g_brk_commit );

    g_DMA = memory.data() + text_base - 0x80; // midway through the base page
    uint32_t data_base = text_base; // + head.cb_text; with 0x601a all bases belong to text_base
    uint32_t bss_base = text_base; // data_base + head.cb_data;

    if ( !handle_relocations_cpm( fp, head, text_base, data_base, bss_base ) )
        return false;

    if ( !read_symbols_cpm( fp, head, text_base, data_base, bss_base ) )
        return false;

    tracer.Trace( "memory map from highest to lowest addresses:\n" );
    tracer.Trace( "  first byte beyond allocated memory:                 %x\n", g_base_address + memory_size );
    tracer.Trace( "  actual top of stack:                                %x\n", g_top_of_stack + 8 );
    tracer.Trace( "  initial stack pointer g_top_of_stack:               %x\n", g_top_of_stack );
    REG_TYPE stack_bytes = g_stack_commit;
    tracer.Trace( "  <stack>                                             (%d == %x bytes)\n", stack_bytes, stack_bytes );
    tracer.Trace( "  last byte stack can use (g_bottom_of_stack):        %x\n", g_base_address + g_bottom_of_stack );
    tracer.Trace( "  <unallocated space between brk and the stack>       (%d == %llx bytes)\n", g_brk_commit, g_brk_commit );
    tracer.Trace( "  end_of_bss / current brk:                           %x\n", g_base_address + g_end_of_data );
    tracer.Trace( "  <uninitialized bss data\n" );
    tracer.Trace( "  start of bss segment:                               %x\n", g_execution_address + head.cb_text + head.cb_data );
    tracer.Trace( "  <initialized data from the .68k file>\n" );
    tracer.Trace( "  start of data segment:                              %x\n", g_execution_address + head.cb_text );
    tracer.Trace( "  <code from the .68k file>\n" );
    tracer.Trace( "  initial pc execution_addess + start of code         %x\n", g_execution_address );
    tracer.Trace( "  start of base page:                                 %x\n", base_page );
    tracer.Trace( "  start of the address space:                         %x\n", g_base_address );

    tracer.Trace( "vm memory first byte beyond:     %p\n", memory.data() + memory_size );
    tracer.Trace( "vm memory start:                 %p\n", memory.data() );
    tracer.Trace( "memory_size:                     %#x == %d\n", memory_size, memory_size );

    tracer.Trace( "first 512 bytes starting at base page:\n" );
    tracer.TraceBinaryData( memory.data() + base_page, 512, 8 );

    return true;
} //load_cpm68k

bool parse_FCB_Filename( FCBCPM68K * pfcb, char * pcFilename )
{
    char * orig = pcFilename;

    // note: the high bits are used for file attributes. Mask them away

    for ( int i = 0; i < 8; i++ )
    {
        if ( ' ' == ( 0x7f & pfcb->f[ i ] ) )
            break;
        *pcFilename++ = ( 0x7f & pfcb->f[ i ] );
    }

    if ( ' ' != pfcb->t[0] )
    {
        *pcFilename++ = '.';

        for ( int i = 0; i < 3; i++ )
        {
            if ( ' ' == ( 0x7f & pfcb->t[ i ] ) )
                break;
            *pcFilename++ = ( 0x7f & pfcb->t[ i ] );
        }
    }

    *pcFilename = 0;

    // CP/M assumes all filenames are uppercase. Linux users generally use all lowercase filenames

    if ( g_forceLowercase )
        _strlwr( orig );

    return ( pcFilename != orig );
} //parse_FCB_Filename

void WriteRandom( m68000 & cpu )
{
    FCBCPM68K * pfcb = (FCBCPM68K *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
    pfcb->Trace();
    ACCESS_REG( REG_RESULT ) = 6; // seek past end of disk

    char acFilename[ CPM_FILENAME_LEN ];
    bool ok = parse_FCB_Filename( pfcb, acFilename );
    if ( ok )
    {
        FILE * fp = FindFileEntry( acFilename );
        if ( fp )
        {
            uint32_t record = pfcb->GetRandomIOOffset();
            uint32_t file_offset = record * (uint32_t) 128;

            fseek( fp, 0, SEEK_END );
            uint32_t file_size = ftell( fp );
            tracer.Trace( "  write random file %p, record %#x, file_offset %d, file_size %d\n", fp, record, file_offset, file_size );

            if ( file_offset > file_size )
            {
                ok = !fseek( fp, file_offset, SEEK_SET );
                if ( ok )
                    file_size = ftell( fp );
                else
                    tracer.Trace( "  can't seek to extend file with zeros, error %d = %s\n", errno, strerror( errno ) );
            }

            if ( file_size >= file_offset )
            {
                ok = !fseek( fp, file_offset, SEEK_SET );
                if ( ok )
                {
                    tracer.Trace( "  writing random at offset %#x\n", file_offset );
                    tracer.TraceBinaryData( g_DMA, 128, 2 );
                    size_t numwritten = fwrite( g_DMA, 128, 1, fp );
                    if ( numwritten )
                    {
                        ACCESS_REG( REG_RESULT ) = 0;

                        // The CP/M spec says random write should set the file offset such
                        // that the following sequential I/O will be from the SAME location
                        // as this random write -- not 128 bytes beyond.

                        fseek( fp, file_offset, SEEK_SET );
                    }
                    else
                        tracer.Trace( "ERROR: can't write in write random, error %d = %s\n", errno, strerror( errno ) );
                }
                else
                    tracer.Trace( "ERROR: can't seek in write random, offset %#x, size %#x\n", file_offset, file_size );
            }
            else
                tracer.Trace( "ERROR: write random at offset %d beyond end of file size %d\n", file_offset, file_size );
        }
        else
            tracer.Trace( "ERROR: write random on unopened file\n" );
    }
    else
        tracer.Trace( "ERROR: write random can't parse filename\n" );
} //WriteRandom

void emulator_invoke_68k_trap3( m68000 & cpu ) // bios
{
    uint16_t function = ( 0xffff & ACCESS_REG( REG_SYSCALL ) );
    tracer.Trace( "trap 3 cp/m 68k bios call %u arguments %#x, %#x\n", function, ACCESS_REG( REG_ARG0 ), ACCESS_REG( REG_ARG1 ) );

    switch( function )
    {
        case 22: // set exception handler address
        {
            uint32_t vector_number = 0xffff & ACCESS_REG( REG_ARG0 );
            uint32_t vector_address = ACCESS_REG( REG_ARG1 );
            ACCESS_REG( REG_RESULT ) = cpu.getui32( vector_number * 4 );
            cpu.setui32( vector_number * 4, vector_address );
            break;
        }
        default:
        {
            printf( "  unhandled cp/m bios call %u\n", function );
            tracer.Trace( "  unhandled cp/m bios call %u\n", function );
            ACCESS_REG( REG_RESULT ) = 0xff;
            break;
        }
    }
} //emulator_invoke_68k_trap3

uint8_t map_input( uint8_t input )
{
    uint8_t output = input;

#if defined(_MSC_VER) || defined(WATCOM)
    // On Windows, input is  0xe0 for standard arrow keys and 0 for keypad equivalents
    // On DOS/WATCOM, input is 0 for both cases

    if ( 0 == input || 0xe0 == input )
    {
        uint8_t next = (uint8_t) ConsoleConfiguration::portable_getch();

        // map various keys to ^ XSEDCRG used in many apps.

        if ( 'K' == next )                   // left arrow
            output = 1 + 'S' - 'A';
        else if ( 'P' == next )              // down arrow
            output = 1 + 'X' - 'A';
        else if ( 'M' == next )              // right arrow
            output = 1 + 'D' - 'A';
        else if ( 'H' == next )              // up arrow
            output = 1 + 'E' - 'A';
        else if ( 'Q' == next )              // page down
            output = 1 + 'C' - 'A';
        else if ( 'I' == next )              // page up
            output = 1 + 'R' - 'A';
        else if ( 'S' == next )              // del
            output = 1 + 'G' - 'A';
        else
            tracer.Trace( "  no map_input mapping for %02x, second character %02x\n", input, next );

        tracer.Trace( "    next character after %02x: %02x == '%c' mapped to %02x\n", input, printable( next ), output );
    }
#else // Linux / MacOS
    if ( 0x1b == input )
    {
        if ( g_consoleConfig.portable_kbhit() )
        {
            tracer.Trace( "read an escape on linux... getting next char\n" );
            uint8_t nexta = ConsoleConfiguration::portable_getch();
            tracer.Trace( "read an escape on linux... getting next char again\n" );
            uint8_t nextb = ConsoleConfiguration::portable_getch();
            tracer.Trace( "  nexta: %02x. nextb: %02x\n", nexta, nextb );

            if ( '[' == nexta )
            {
                if ( 'A' == nextb )              // up arrow
                    output = 1 + 'E' - 'A';
                else if ( 'B' == nextb )         // down arrow
                    output = 1 + 'X' - 'A';
                else if ( 'C' == nextb )         // right arrow
                    output = 1 + 'D' - 'A';
                else if ( 'D' == nextb )         // left arrow
                    output = 1 + 'S' - 'A';
                else if ( '5' == nextb )         // page up
                {
                    uint8_t nextc = g_consoleConfig.portable_getch();
                    tracer.Trace( "  5 nextc: %02x\n", nextc );
                    if ( '~' == nextc )
                        output = 1 + 'R' - 'A';
                }
                else if ( '6' == nextb )         // page down
                {
                    uint8_t nextc = g_consoleConfig.portable_getch();
                    tracer.Trace( "  6 nextc: %02x\n", nextc );
                    if ( '~' == nextc )
                        output = 1 + 'C' - 'A';
                }
                else if ( '3' == nextb )         // DEL on linux
                {
                    uint8_t nextc = g_consoleConfig.portable_getch();
                    tracer.Trace( "  3 nextc: %02x\n", nextc );
                    if ( '~' == nextc )
                        output = 0x7f;
                }
                else
                    tracer.Trace( "unhandled nextb %u == %02x\n", nextb, nextb ); // lots of other keys not on a cp/m machine here
            }
            else
                tracer.Trace( "unhandled linux keyboard escape sequence\n" );
        }
    }
#endif

    return output;
} //map_input

void emulator_invoke_68k_trap2( m68000 & cpu ) // bdos
{
    char acFilename[ CPM_FILENAME_LEN ];
    uint16_t function = ( 0xffff & ACCESS_REG( REG_SYSCALL ) );
    tracer.Trace( "trap 2 cp/m 68k bdos call %u, argument %#x -- %s\n", function, ACCESS_REG( REG_ARG0 ), bdos_function( function ) );

    // note: only the subset invoked by cp/m 68k versions of the C, BASIC, and Pascal compilers, assembler, and linker are implemented.

    switch ( function )
    {
        case 0: // system reset; exit the app
        {
            g_terminate = true;
            cpu.end_emulation();
            g_exit_code = (int) ACCESS_REG( REG_ARG0 ); // not part of the cp/m 68k spec but it seems handy
            tracer.Trace( "  emulated app exit code %d\n", g_exit_code );
            break;
        }
        case 1: // console input. echo input to console
        {
            uint8_t ch = (uint8_t) get_next_kbd_char();
            ACCESS_REG( REG_RESULT ) = map_input( ch );
            tracer.Trace( "  bdos console in: %02x == '%c'\n", ch, printable( ch ) );
            printf( "%c", ch );
            fflush( stdout );
            break;
        }
        case 2: // console output. d1.w ascii chracter
        {
            // console output
            // CP/M 2.2 checks for ^s and ^q to pause and resume output. If output is paused due to ^s,
            // a subsequent ^c terminates the application. ^q resumes output then ^c has no effect.

            uint8_t ch = ( 0xff & ACCESS_REG( REG_ARG0 ) );
            if ( 0x0d != ch && 0 != ch )  // skip carriage return because line feed turns into cr+lf. Also, cp/m 68k as.68k v1 outputs a null character; ignore it.
            {
                tracer.Trace( "  bdos console out: %02x == '%c'\n", ch, printable( ch ) );
                printf( "%c", ch );
                fflush( stdout );
            }

            ACCESS_REG( REG_RESULT ) = 0;
            break;
        }
        case 6: // direct console i/o. Note slightly different behavior than cp/m 80 v2.2 with the 0xff and 0xfe distinction
        {
            uint16_t cmd = (uint16_t) ( 0xffff & ACCESS_REG( REG_ARG0 ) );
            if ( 0xff == cmd ) // input. block until a character is ready
            {
                uint8_t input = (uint8_t) get_next_kbd_char();
                tracer.Trace( "  read character %u == %02x == '%c'\n", input, input, printable( input ) );
                ACCESS_REG( REG_RESULT ) = map_input( input );
            }
            else if ( 0xfe == cmd ) // status. return non-zero if available and 0 if unavailable
            {
                if ( is_kbd_char_available() )
                    ACCESS_REG( REG_RESULT ) = 1;
                else
                    ACCESS_REG( REG_RESULT ) = 0;
            }
            else // output
            {
                uint8_t ch = ( 0xff & ACCESS_REG( REG_ARG0 ) );
                if ( 0x0d != ch )             // skip carriage return because line feed turns into cr+lf
                {
                    tracer.Trace( "  bdos console i/o output: %02x == '%c'\n", ch, printable( ch ) );
                    printf( "%c", ch );
                    fflush( stdout );
                }
            }
            break;
        }
        case 9: // send $-terminated string to stdout
        {
            char * str = (char *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
            char * pdollar = strchr( str, '$' );
            if ( pdollar )
                tracer.Trace( "   string: %.*s\n", pdollar - str, str );
            uint32_t count = 0;
            while ( '$' != *str )
            {
                if ( count++ > 2000 ) // arbitrary limit, but probably a bug if this long
                {
                    tracer.Trace( "  ERROR: String to print is too long!\n", stderr );
                    break;
                }

                uint8_t ch = *str++;
                if ( 0x0d != ch )              // skip carriage return because line feed turns into cr+lf
                    printf( "%c", ch );
            }
            fflush( stdout );
            break;
        }
        case 10: // read console buffer. aka buffered console input. DE: buffer address. buffer:
        {
            //   0      1         2       3
            //   in_len out_len   char1   char2 ...

            char * pbuf = (char *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
            pbuf[ 1 ] = 0;
            uint16_t in_len = *pbuf;

            if ( in_len > 0 )
            {
                pbuf[ 2 ] = 0;
                uint8_t out_len;
                bool reboot = cpm_read_console( pbuf + 2, in_len, out_len );
                if ( reboot )
                {
                    tracer.Trace( "  bdos read console buffer read a ^c at the first position, so it's terminating the app\n" );
                    cpu.end_emulation();
                    g_terminate = true;
                    g_exit_code = 1;
                    break;
                }

                pbuf[ 1 ] = out_len;
                tracer.Trace( "  read console len %u, string '%.*s'\n", out_len, (size_t) out_len, pbuf + 2 );
            }
            else
                tracer.Trace( "WARNING: read console buffer asked for input but provided a 0-length buffer\n" );
            break;
        }
        case 11: // get console status
        {
            // for now lie and say no characters are available

            ACCESS_REG( REG_RESULT ) = 0;
            break;
        }
        case 12: // return version number
        {
            ACCESS_REG( REG_RESULT ) = 0x2022; // cp/m-68k v1.1
            break;
        }
        case 15: // open file. success 0-3, error 0xff
        {
            FCBCPM68K * pfcb = (FCBCPM68K *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
            tracer.TraceBinaryData( (uint8_t *) pfcb, sizeof( FCBCPM68K ), 4 );
            pfcb->Trace();
            ACCESS_REG( REG_RESULT ) = 255;
            bool ok = parse_FCB_Filename( pfcb, acFilename );
            if ( ok )
            {
                tracer.Trace( "  opening file '%s' for pfcb %p\n", acFilename, pfcb );
                FILE * fp = FindFileEntry( acFilename );
                if ( fp )
                {
                    // sometimes apps like the CP/M assembler ASM open files that are already open.
                    // rewind it to position 0.

                    fseek( fp, 0, SEEK_SET );
                    ACCESS_REG( REG_RESULT ) = 0;
                    pfcb->cr = 0;
                    pfcb->SetRecordCount( fp ); // Whitesmith C 2.1 requires this to be set
                    // don't reset extent on a re-open or LK80.COM will fail. pfcb->ex = 0;
                    pfcb->s2 = 0;
                    tracer.Trace( "  open used existing file and rewound to offset 0\n" );
                }
                else
                {
                    // the cp/m 2.2 spec says that filenames may contain question marks. I haven't found an app that uses that.

                    fp = fopen( acFilename, "r+b" );
                    if ( fp )
                    {
                        FileEntry fe;
                        strcpy( fe.acName, acFilename );
                        fe.fp = fp;
                        g_fileEntries.push_back( fe );
                        ACCESS_REG( REG_RESULT ) = 0;

                        // Digital Research's lk80.com linker has many undocumented expectations no other apps I've tested have.
                        // including rc > 0 after an open. Whitesmith C requires the record count set correctly.

                        pfcb->cr = 0;
                        pfcb->SetRecordCount( fp );
                        pfcb->ex = 0;
                        pfcb->s2 = 0;
                        tracer.Trace( "  file opened successfully, record count: %u\n", pfcb->rc );
                    }
                    else
                        tracer.Trace( "ERROR: can't open file '%s' error %d = %s\n", acFilename, errno, strerror( errno ) );
                }
            }
            else
                tracer.Trace( "ERROR: can't parse filename in FCB\n" );
            break;
        }
        case 16: // close file. return 255 on error and 0..3 directory code otherwise
        {
            FCBCPM68K * pfcb = (FCBCPM68K *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
            pfcb->Trace();
            ACCESS_REG( REG_RESULT ) = 255;
            bool ok = parse_FCB_Filename( pfcb, acFilename );
            if ( ok )
            {
                FILE * fp = RemoveFileEntry( acFilename );
                if ( fp )
                {
                    int ret = fclose( fp );

                    if ( 0 == ret )
                        ACCESS_REG( REG_RESULT ) = 0;
                    else
                        tracer.Trace( "ERROR: file close failed, error %d = %s\n", errno, strerror( errno ) );
                }
                else
                    tracer.Trace( "ERROR: file close on file that's not open\n" );
            }
            else
                tracer.Trace( "ERROR: can't parse filename in close call\n" );
            break;
        }
        case 19: // delete file. return 255 if file not found and 0..3 directory code otherwise
        {
            FCBCPM68K * pfcb = (FCBCPM68K *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
            pfcb->Trace();
            ACCESS_REG( REG_RESULT ) = 255;
            bool ok = parse_FCB_Filename( pfcb, acFilename );
            if ( ok )
            {
                // if deleting an open file, close it first. CalcStar does this on file save.

                if ( FindFileEntry( acFilename ) )
                {
                    FILE * fp = RemoveFileEntry( acFilename );
                    if ( fp )
                        fclose( fp );
                }

                int removeok = ( 0 == remove( acFilename ) );
                tracer.Trace( "  attempt to remove file '%s' result ok: %d\n", acFilename, removeok );
                if ( removeok )
                    ACCESS_REG( REG_RESULT ) = 0;
                else
                    tracer.Trace( "  error %d = %s\n", errno, strerror( errno ) );
            }
            else
                tracer.Trace( "ERROR: can't parse filename for delete file\n" );
            break;
        }
        case 20: // read sequential. return 0 on success or non-0 on failure:
        {
            // reads 128 bytes from cr of the extent and increments cr.
            // if cr overflows, the extent is incremented and cr is set to 0 for the next read

            FCBCPM68K * pfcb = (FCBCPM68K *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
            pfcb->Trace();
            ACCESS_REG( REG_RESULT ) = 255;
            bool ok = parse_FCB_Filename( pfcb, acFilename );
            if ( ok )
            {
                FILE * fp = FindFileEntry( acFilename );
                if ( fp )
                {
                    // apps like Digital Research's linker LK80 update pcb->cr and pfcb->ex between
                    // calls to sequential read and effectively does random reads using sequential I/O.
                    // This is illegal (the doc says apps can't touch this data, but that ship has sailed and sunk).

                    uint32_t file_size = portable_filelen( fp );
                    uint32_t curr = pfcb->GetSequentialOffset();
                    uint16_t dmaOffset = (uint16_t) ( g_DMA - memory.data() );
                    tracer.Trace( "  file size: %#x = %u, current %#x = %u, dma %#x = %u\n",
                                  file_size, file_size, curr, curr, dmaOffset, dmaOffset );

                    if ( curr < file_size )
                    {
                        fseek( fp, curr, SEEK_SET );

                        uint32_t to_read = get_min( file_size - curr, (uint32_t) 128 );
                        memset( g_DMA, 0x1a, 128 ); // fill with ^z, the EOF marker in CP/M

                        size_t numread = fread( g_DMA, 1, to_read, fp );
                        if ( numread > 0 )
                        {
                            tracer.TraceBinaryData( g_DMA, 128, 2 );
                            ACCESS_REG( REG_RESULT ) = 0;
                            pfcb->UpdateSequentialOffset( curr + 128 );
                        }
                        else
                        {
                            ACCESS_REG( REG_RESULT ) = 1;
                            tracer.Trace( "  read error %d = %s, so returning a = 1\n", errno, strerror( errno ) );
                        }
                    }
                    else
                    {
                        ACCESS_REG( REG_RESULT ) = 1;
                        tracer.Trace( "  at the end of file, so returning a = 1\n" );
                    }
                }
                else
                    tracer.Trace( "ERROR: can't read from a file that's not open\n" );
            }
            else
                tracer.Trace( "ERROR: can't parse filename in read sequential file\n" );
            break;
        }
        case 21: // write sequential. return 0 on success or non-0 on failure (out of disk space)
        {
            // reads 128 bytes from cr of the extent and increments cr.
            // if cr overflows, the extent is incremented and cr is set to 0 for the next read

            FCBCPM68K * pfcb = (FCBCPM68K *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
            pfcb->Trace();
            ACCESS_REG( REG_RESULT ) = 255;
            bool ok = parse_FCB_Filename( pfcb, acFilename );
            if ( ok )
            {
                FILE * fp = FindFileEntry( acFilename );
                if ( fp )
                {
                    uint32_t file_size = portable_filelen( fp );
                    uint32_t curr = pfcb->GetSequentialOffset();
                    uint16_t dmaOffset = (uint16_t) ( g_DMA - memory.data() );
                    tracer.Trace( "  writing at offset %#x = %u, file size is %#x = %u, dma %#x = %u\n",
                                  curr, curr, file_size, file_size, dmaOffset, dmaOffset );
                    fseek( fp, curr, SEEK_SET );

                    tracer.TraceBinaryData( g_DMA, 128, 2 );
                    size_t numwritten = fwrite( g_DMA, 128, 1, fp );
                    if ( numwritten > 0 )
                    {
                        ACCESS_REG( REG_RESULT ) = 0;
                        pfcb->UpdateSequentialOffset( curr + 128 );
                        pfcb->SetRecordCount( fp ); // Whitesmith C v2.1's A80.COM assembler requires this
                    }
                    else
                        tracer.Trace( "ERROR: fwrite returned %zd, errno %d = %s\n", numwritten, errno, strerror( errno ) );
                }
                else
                    tracer.Trace( "ERROR: can't write to a file that's not open\n" );
            }
            else
                tracer.Trace( "ERROR: can't parse filename in write sequential file\n" );
            break;
        }
        case 22: // make file. return 255 if out of space or the file exists. 0..3 directory code otherwise.
        {
            // "the Make function has the side effect of activating the FCB and thus a subsequent open is not necessary."

            FCBCPM68K * pfcb = (FCBCPM68K *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
            pfcb->Trace();
            ACCESS_REG( REG_RESULT ) = 255;
            bool ok = parse_FCB_Filename( pfcb, acFilename );
            if ( ok )
            {
                tracer.Trace( "  making file '%s'\n", acFilename );
                FILE * fp = fopen( acFilename, "w+b" );
                if ( fp )
                {
                    FileEntry fe;
                    strcpy( fe.acName, acFilename );
                    fe.fp = fp;
                    g_fileEntries.push_back( fe );

                    pfcb->cr = 0;
                    pfcb->rc = 0;
                    pfcb->ex = 0;
                    pfcb->s2 = 0;

                    tracer.Trace( "  successfully created fp %p for write\n", fp );
                    ACCESS_REG( REG_RESULT ) = 0;
                }
                else
                    tracer.Trace( "ERROR: unable to make file\n" );
            }
            else
                tracer.Trace( "ERROR: can't parse filename in make file\n" );
            break;
        }
        case 25: // return current disk
        {
            ACCESS_REG( REG_RESULT ) = 0;
            break;
        }
        case 26: // set the dma address (128 byte buffer for doing I/O)
        {
            tracer.Trace( "  updating DMA address; D %u = %#x\n", ACCESS_REG( REG_ARG0 ), ACCESS_REG( REG_ARG0 ) );
            g_DMA = memory.data() + ACCESS_REG( REG_ARG0 );
            break;
        }
        case 32: // get/set current user
        {
            ACCESS_REG( REG_RESULT ) = 0;
            break;
        }
        case 33: // read random
        {
            FCBCPM68K * pfcb = (FCBCPM68K *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
            pfcb->Trace();
            ACCESS_REG( REG_RESULT ) = 6; // seek past end of disk
            bool ok = parse_FCB_Filename( pfcb, acFilename );
            if ( ok )
            {
                FILE * fp = FindFileEntry( acFilename );
                if ( fp )
                {
                    uint32_t record = pfcb->GetRandomIOOffset();
                    tracer.Trace( "  read random record %u == %#x\n", record, record );
                    uint32_t file_offset = (uint32_t) record * (uint32_t) 128;
                    memset( g_DMA, 0x1a, 128 ); // fill with ^z, the EOF marker in CP/M

                    uint32_t file_size = portable_filelen( fp );

                    // OS workaround for app bug: Turbo Pascal expects a read just past the end of file to succeed.

                    if ( file_size == file_offset )
                    {
                        tracer.Trace( "  random read at eof, offset %u\n", file_size );
                        ACCESS_REG( REG_RESULT ) = 1;
                        break;
                    }

                    if ( file_size > file_offset )
                    {
                        uint32_t to_read = get_min( file_size - file_offset, (uint32_t) 128 );
                        ok = !fseek( fp, file_offset, SEEK_SET );
                        if ( ok )
                        {
                            tracer.Trace( "  reading random at offset %u == %#x. file size %u, to read %u\n", file_offset, file_offset, file_size, to_read );
                            size_t numread = fread( g_DMA, 1, to_read, fp );
                            if ( numread )
                            {
                                tracer.TraceBinaryData( g_DMA, to_read, 2 );
                                ACCESS_REG( REG_RESULT ) = 0;

                                // The CP/M spec says random read should set the file offset such
                                // that the following sequential I/O will be from the SAME location
                                // as this random read -- not 128 bytes beyond.

                                pfcb->UpdateSequentialOffset( file_offset );
                            }
                            else
                                tracer.Trace( "ERROR: can't read in read random\n" );
                        }
                        else
                            tracer.Trace( "ERROR: can't seek in read random\n" );
                    }
                    else
                    {
                        ACCESS_REG( REG_RESULT ) = 1;
                        tracer.Trace( "ERROR: read random read at %u beyond end of file size %u\n", file_offset, file_size );
                    }
                }
                else
                    tracer.Trace( "ERROR: read random on unopened file\n" );
            }
            else
                tracer.Trace( "ERROR: read random can't parse filename\n" );
            break;
        }
        case 34: // write random
        {
            WriteRandom( cpu );
            break;
        }
        case 35: // Compute file size. A = 0 if ok, 0xff on failure. Sets r0/r1/2 to the number of 128 byte records
        {
            FCBCPM68K * pfcb = (FCBCPM68K *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
            pfcb->Trace();
            ACCESS_REG( REG_RESULT ) = 255;
            bool ok = parse_FCB_Filename( pfcb, acFilename );
            if ( ok )
            {
                FILE * fp = FindFileEntry( acFilename );
                bool found = false;
                if ( fp )
                    found = true;
                else
                    fp = fopen( acFilename, "r+b" );

                if ( fp )
                {
                    uint32_t file_size = portable_filelen( fp );
                    file_size = round_up( file_size, (uint32_t) 128 ); // cp/m files round up in 128 byte records
                    pfcb->SetRandomIOOffset( file_size / 128 );
                    ACCESS_REG( REG_RESULT ) = 0;
                    tracer.Trace( "  file size is %u == %u records; r2 %#x r1 %#x r0 %#x\n", file_size, file_size / 128, pfcb->r2, pfcb->r1, pfcb->r0 );

                    if ( !found )
                        fclose( fp );
                }
                else
                    tracer.Trace( "ERROR: compute file size can't find file '%s'\n", acFilename );
            }
            else
                tracer.Trace( "ERROR: compute file size can't parse filename\n" );
            break;
        }
        case 40:
        {
            // write random with zero fill.
            // Just like 34 / write random except also fills any file-extending blocks with 0.
            // The 34 implementation of WriteRandom already fills with zeros, so just use that.
            // I haven't found any apps that call this, so I can't say it's really tested.

            WriteRandom( cpu );
            break;
        }
        case 47: // chain to program
        {
            tracer.Trace( "chain to len %u command '%s'\n", * g_DMA, g_DMA + 1 );
            char * pcmdline = (char *) ( g_DMA + 1 );
            char * ptail = strchr( pcmdline, ' ' );
            if ( ptail )
            {
                *ptail = 0;
                ptail++;
            }
            else
                ptail = pcmdline + strlen( pcmdline );

            char acApp[ CPM_FILENAME_LEN ];
            strcpy( acApp, pcmdline );
            char acAppArgs[ 128 ];
            strcpy( acAppArgs, ptail );

            if ( load_cpm68k( acApp, acAppArgs ) )
            {
                tracer.Trace( "loaded chained app successfully\n" );
                cpu.reset( memory, g_base_address, g_execution_address, g_stack_commit, g_top_of_stack );
            }
            else
            {
                ACCESS_REG( REG_RESULT ) = 0xff;
                tracer.Trace( "unable to load chained app\n" );
            }
            break;
        }
        case 48: // flush buffers
        {
            fflush( 0 );
            ACCESS_REG( REG_RESULT ) = 0;
            break;
        }
        case 59: // program load (used by ddt). returns 0 = success, 1 = insufficient RAM, 2 = read error, 3 = bad relocation bits
        {
            LoadParameterBlockCPM * plpb = (LoadParameterBlockCPM *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
            plpb->swap_endianness();
            plpb->trace();
            ACCESS_REG( REG_RESULT ) = 1;

            bool ok = parse_FCB_Filename( (FCBCPM68K *) cpu.getmem( plpb->fcbOfChildApp ), acFilename );
            if ( ok )
            {
                tracer.Trace( "  program to load: '%s'\n", acFilename );
                FILE * fp = FindFileEntry( acFilename );
                if ( fp )
                {
                    uint32_t basePage = 0;
                    uint32_t stackPointer = 0;
                    if ( load59_cpm68k( fp, plpb->lowestAddress, plpb->highestAddress, plpb->loaderControlFlags, basePage, stackPointer ) )
                    {
                        plpb->childBasePage = basePage;
                        plpb->childStackPointer = stackPointer;
                        plpb->trace();
                        plpb->swap_endianness();
                        cpu.relax_pc_sp_constraints();
                        ACCESS_REG( REG_RESULT ) = 0;
                    }
                    else
                        tracer.Trace( "ERROR: program load failed to actually load the app\n" );
                }
                else
                    tracer.Trace( "ERROR: program load can't find file in list of open files\n" );
            }
            else
                tracer.Trace( "ERROR: program load can't parse filename\n" );
            break;
        }
        case 61: // set exception vector. returns 0 on success, 0xff on failure
        {
            ExceptionParameterBlockCPM * pepb = (ExceptionParameterBlockCPM *) cpu.getmem( ACCESS_REG( REG_ARG0 ) );
            tracer.TraceBinaryData( (uint8_t *) pepb, 10, 8 );
            pepb->swap_endianness();
            pepb->trace();

            if ( pepb->vector >= 64 )
            {
                tracer.Trace( "ERROR: invalid vector number %u\n", pepb->vector );
                ACCESS_REG( REG_RESULT ) = 0xff;
                break;
            }

            pepb->oldValue = cpu.getui32( pepb->vector * 4 );
            cpu.setui32( pepb->vector * 4, pepb->newValue );
            pepb->trace();
            pepb->swap_endianness();
            ACCESS_REG( REG_RESULT ) = 0;
            break;
        }
        case 62: // set supervisor state.
        {
            cpu.set_supervisor_state();
            ACCESS_REG( REG_RESULT ) = 0;
            break;
        }
        default:
        {
            printf( "  unhandled cp/m bdos call %u\n", function );
            tracer.Trace( "  unhandled cp/m bdos call %u\n", function );
            break;
        }
    }
} //emulator_invoke_68k_trap2

static bool load_image32( FILE * fp, const char * pimage, const char * app_args )
{
    ElfHeader32 ehead = {0};
    fseek( fp, (long) 0, SEEK_SET );
    size_t read = fread( &ehead, 1, sizeof ehead, fp );

    if ( 0x464c457f != ehead.magic && 0x7f454c46 != ehead.magic )
        usage( "elf image file's magic header is invalid" );

    bool big_endian = ( 2 == ehead.endianness );
    tracer.Trace( "image is %s endian\n", big_endian ? "big" : "little" );
    if ( big_endian == CPU_IS_LITTLE_ENDIAN )
        usage( "elf image endianness isn't consistent with emulator expectations" );

    ehead.swap_endianness();

    if ( 2 != ehead.type )
    {
        printf( "e_type is %d == %s\n", ehead.type, image_type( ehead.type ) );
        usage( "elf image isn't an executable file (2 expected)" );
    }

    if ( ELF_MACHINE_ISA != ehead.machine )
        usage( "elf image machine ISA doesn't match this emulator" );

    if ( 0 == ehead.entry_point )
        usage( "elf entry point is 0, which is invalid" );

    tracer.Trace( "header fields:\n" );
    tracer.Trace( "  entry address: %x\n", ehead.entry_point );
    tracer.Trace( "  program entries: %u\n", ehead.program_header_table_entries );
    tracer.Trace( "  program header entry size: %u\n", ehead.program_header_table_size );
    tracer.Trace( "  program offset: %u == %x\n", ehead.program_header_table, ehead.program_header_table );
    tracer.Trace( "  section entries: %u\n", ehead.section_header_table_entries );
    tracer.Trace( "  section header entry size: %u\n", ehead.section_header_table_size );
    tracer.Trace( "  section offset: %u == %x\n", ehead.section_header_table, ehead.section_header_table );
    tracer.Trace( "  flags: %x\n", ehead.flags );
    g_execution_address = ehead.entry_point;

    // determine how much RAM to allocate

    REG_TYPE memory_size = 0;

    for ( uint16_t ph = 0; ph < ehead.program_header_table_entries; ph++ )
    {
        size_t o = ehead.program_header_table + ( ph * ehead.program_header_table_size );
        tracer.Trace( "program header %u at offset %zu\n", ph, o );

        ElfProgramHeader32 head = {0};
        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, get_min( sizeof( head ), (size_t) ehead.program_header_table_size ), 1, fp );
        if ( 1 != read )
            usage( "can't read program header" );

        if ( big_endian )
            head.swap_endianness();

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

        REG_TYPE just_past = head.physical_address + head.memory_size;
        if ( just_past > memory_size )
            memory_size = just_past;

        if ( ( 0 != head.physical_address ) && ( ( 0 == g_base_address ) || g_base_address > head.physical_address ) )
            g_base_address = head.physical_address;
    }

    memory_size -= g_base_address;
    tracer.Trace( "memory_size of content to load from elf file: %x\n", memory_size );

    // first load the string table

    for ( uint16_t sh = 0; sh < ehead.section_header_table_entries; sh++ )
    {
        size_t o = ehead.section_header_table + ( sh * ehead.section_header_table_size );
        ElfSectionHeader32 head = {0};

        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, 1, get_min( sizeof( head ), (size_t) ehead.section_header_table_size ), fp );
        if ( 0 == read )
            usage( "can't read section header" );

        if ( big_endian )
            head.swap_endianness();

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

        ElfSectionHeader32 head = {0};

        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, 1, get_min( sizeof( head ), (size_t) ehead.section_header_table_size ), fp );
        if ( 0 == read )
            usage( "can't read section header" );

        if ( big_endian )
            head.swap_endianness();

        tracer.Trace( "  type: %x / %s\n", head.type, head.show_type() );
        tracer.Trace( "  flags: %x / %s\n", head.flags, head.show_flags() );
        tracer.Trace( "  address: %x\n", head.address );
        tracer.Trace( "  offset: %x\n", head.offset );
        tracer.Trace( "  size: %x\n", head.size );

        if ( 2 == head.type )
        {
            g_symbols32.resize( head.size / sizeof( ElfSymbol32 ) );
            fseek( fp, (long) head.offset, SEEK_SET );
            read = fread( g_symbols32.data(), 1, head.size, fp );
            if ( 0 == read )
                usage( "can't read symbol table\n" );
        }
    }

    // void out the entries that don't have symbol names or have mangled names that start with $

    for ( size_t se = 0; se < g_symbols32.size(); se++ )
    {
        if ( big_endian )
            g_symbols32[se].swap_endianness();

        if ( ( 0 == g_symbols32[se].name ) || ( '$' == g_string_table[ g_symbols32[se].name ] ) )
            g_symbols32[se].value = 0;
    }

    // use known my_qsort so traces are consistent across platforms because qsort implementations for duplicate values differ
    tracer.Trace( "sorting invalid symbol entries\n" );
    my_qsort( g_symbols32.data(), g_symbols32.size(), sizeof( ElfSymbol32 ), symbol_compare32 );

    // remove symbols that don't look like they have a valid addresses (rust binaries have tens of thousands of these)

    size_t to_erase = 0;
    for ( size_t se = 0; se < g_symbols32.size(); se++ )
    {
        if ( g_symbols32[ se ].value < g_base_address )
            to_erase++;
        else
            break;
    }

    if ( to_erase > 0 )
        g_symbols32.erase( g_symbols32.begin(), g_symbols32.begin() + to_erase );

    // set the size of each symbol if it's not already set

    for ( size_t se = 0; se < g_symbols32.size(); se++ )
    {
        if ( 0 == g_symbols32[se].size )
        {
            if ( se < ( g_symbols32.size() - 1 ) )
                g_symbols32[se].size = g_symbols32[ se + 1 ].value - g_symbols32[ se ].value;
            else
                g_symbols32[se].size = g_base_address + memory_size - g_symbols32[ se ].value;
        }
    }

    tracer.Trace( "elf image has %zu usable symbols:\n", g_symbols32.size() );
    tracer.Trace( "     address      size  name\n" );

    for ( size_t se = 0; se < g_symbols32.size(); se++ )
        tracer.Trace( "    %8x  %8x  %s\n", g_symbols32[ se ].value, g_symbols32[ se ].size, & g_string_table[ g_symbols32[ se ].name ] );

    // memory map from high to low addresses:
    //     <end of allocated memory>
    //     (memory for mmap fulfillment)
    //     g_mmap_offset
    //     (wasted space so g_mmap_offset is 4k-aligned)
    //     arg_data_offset -- actual arg and env, etc. values pointed to by Linux start data
    //     Linux start data on the stack (see details below)
    //     g_top_of_stack
    //     g_bottom_of_stack
    //     (unallocated space between brk and the bottom of the stack)
    //     g_brk_offset with uninitialized RAM (just after arg_data_offset initially)
    //     g_end_of_data
    //     uninitalized data bss (size read from the .elf file)
    //     initialized data (size & data read from the .elf file)
    //     code (read from the .elf file)
    //     g_base_address (offset read from the .elf file).

    // stacks by convention on arm64 and risc-v are 16-byte aligned. make sure to start aligned. waste some space for 68000

    if ( memory_size & 0xf )
    {
        memory_size += 16;
        memory_size &= ~0xf;
    }

    g_end_of_data = memory_size;
    g_brk_offset = memory_size;
    g_highwater_brk = memory_size;
    memory_size += g_brk_commit;

    g_bottom_of_stack = memory_size;
    memory_size += g_stack_commit;
    REG_TYPE top_of_aux = memory_size;

    REG_TYPE arg_data_offset = memory_size;
    memory_size += g_arg_data_commit;

    memory_size = round_up( memory_size, (REG_TYPE) 4096 ); // mmap should hand out 4k-aligned pages
    g_mmap_offset = memory_size;
    memory_size += g_mmap_commit;

    memory.resize( memory_size );
    memset( memory.data(), 0, memory_size );

    g_mmap.initialize( g_base_address + g_mmap_offset, g_mmap_commit, memory.data() - g_base_address );

    // load the program into RAM

    REG_TYPE first_uninitialized_data = 0;

    for ( uint16_t ph = 0; ph < ehead.program_header_table_entries; ph++ )
    {
        size_t o = ehead.program_header_table + ( ph * ehead.program_header_table_size );
        ElfProgramHeader32 head = {0};
        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, 1, get_min( sizeof( head ), (size_t) ehead.program_header_table_size ), fp );
        head.swap_endianness();

        // head.type 1 == load. Other entries will overlap and even have physical addresses, but they are redundant

        if ( 0 != head.file_size && 1 == head.type )
        {
            fseek( fp, (long) head.offset_in_image, SEEK_SET );
            read = fread( memory.data() + head.physical_address - g_base_address, 1, head.file_size, fp );
            if ( 0 == read )
                usage( "can't read image" );

            first_uninitialized_data = get_max( head.physical_address + head.file_size, first_uninitialized_data );

            tracer.Trace( "  read type %s: %x bytes into physical address %x - %x then uninitialized to %llx \n", head.show_type(), head.file_size,
                          head.physical_address, head.physical_address + head.file_size - 1, head.physical_address + head.memory_size - 1 );
            tracer.TraceBinaryData( memory.data() + head.physical_address - g_base_address, get_min( (uint32_t) head.file_size, (uint32_t) 128 ), 4 );
        }
    }

    // write the command-line arguments into the vm memory in a place where _start can find them.
    // there's an array of pointers to the args followed by the arg strings at offset arg_data_offset.

    const uint32_t max_args = 40;
    REG_TYPE aargs[ max_args ]; // vm pointers to each arguments
    char * buffer_args = (char *) ( memory.data() + arg_data_offset );
    size_t image_len = strlen( pimage );
    vector<char> full_command( 2 + image_len + strlen( app_args ) );
    strcpy( full_command.data(), pimage );
    backslash_to_slash( full_command.data() );
    full_command[ image_len ] = ' ';
    strcpy( full_command.data() + image_len + 1, app_args );
    strcpy( buffer_args, full_command.data() );
    char * pargs = buffer_args;
    REG_TYPE args_len = (REG_TYPE) strlen( buffer_args );

    REG_TYPE app_argc = 0;
    while ( *pargs && app_argc < max_args )
    {
        while ( ' ' == *pargs )
            pargs++;

        char * space = strchr( pargs, ' ' );
        if ( space )
            *space = 0;

        REG_TYPE offset = (REG_TYPE) ( pargs - buffer_args );
        tracer.Trace( "offset %x\n", offset );
        aargs[ app_argc ] = swap_endian32( offset + g_base_address + arg_data_offset );
        tracer.Trace( "  argument %d is '%s', at vm address %llx\n", app_argc, pargs, (uint64_t) offset + g_base_address + arg_data_offset );

        app_argc++;
        pargs += strlen( pargs );

        if ( space )
            pargs++;
    }

    REG_TYPE env_offset = args_len + 1;
    tracer.Trace( "env_offset: %llx\n", (uint64_t) env_offset );
    char * penv_data = (char *) ( buffer_args + env_offset );
    strcpy( penv_data, "OS=" );
    strcat( penv_data, APP_NAME );
    REG_TYPE env_os_address = (REG_TYPE) ( penv_data - (char *) memory.data() ) + g_base_address;
    tracer.Trace( "env_os_address %x\n", env_os_address );
    REG_TYPE env_count = 1;
    REG_TYPE env_tz_address = 0;

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
            env_tz_address = (REG_TYPE) ( ptz_data - (char *) memory.data() ) + g_base_address;
            tracer.Trace( "env_tz_address %x\n", env_tz_address );
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
    tracer.TraceBinaryData( (uint8_t *) ( memory.data() + arg_data_offset ), g_arg_data_commit + 0x20, 4 ); // +20 to inspect for bugs

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

    tracer.Trace( "top of aux: %x\n", top_of_aux );
    REG_TYPE * pstack = (REG_TYPE *) ( memory.data() + top_of_aux );

    pstack--;
    *pstack = (REG_TYPE) rand64();
    pstack--;
    *pstack = (REG_TYPE) rand64();
    REG_TYPE prandom = g_base_address + memory_size - ( 2 * sizeof( REG_TYPE ) );

    // ensure that after all of this the stack is 16-byte aligned

    if ( 0 == ( 1 & ( app_argc + env_count ) ) )
        pstack--;

    pstack -= sizeof( AuxProcessStart32 ); // the AT_NULL record will be here since memory is initialized to 0

    pstack -= ( 8 * sizeof( AuxProcessStart32 ) ); // for 8 aux records
    AuxProcessStart32 * paux = (AuxProcessStart32 *) pstack;
    paux[0].a_type = 25; // AT_RANDOM
    paux[0].a_un.a_val = prandom;
    paux[0].swap_endianness();
    paux[1].a_type = 6; // AT_PAGESZ
    paux[1].a_un.a_val = 4096;
    paux[1].swap_endianness();
    paux[2].a_type = 16; // AT_HWCAP
    paux[2].a_un.a_val = 0xa01; // ARM64 bits for fp(0), atomics(8), cpuid(11)
    paux[2].swap_endianness();
    paux[3].a_type = 26; // AT_HWCAP2
    paux[3].a_un.a_val = 0;
    paux[3].swap_endianness();
    paux[4].a_type = 11; // AT_UID
    paux[4].a_un.a_val = 0x595a5449; // "ITZY" they are each my bias.
    paux[4].swap_endianness();
    paux[5].a_type = 22; // AT_EUID;
    paux[5].a_un.a_val = 0x595a5449;
    paux[5].swap_endianness();
    paux[6].a_type = 13; // AT_GID
    paux[6].a_un.a_val = 0x595a5449;
    paux[6].swap_endianness();
    paux[7].a_type = 14; // AT_EGID
    paux[7].a_un.a_val = 0x595a5449;
    paux[7].swap_endianness();

    pstack--; // end of environment data is 0
    pstack--; // move to where the OS environment variable is set OS=RVOS or OS=ARMOS
    *pstack = swap_endian32( env_os_address ); // (REG_TYPE) ( env_offset + arg_data_offset + g_base_address + max_args * sizeof( REG_TYPE ) );
    tracer.Trace( "the OS environment argument is at VM address %llx\n", swap_endian32( *pstack ) );

    if ( 0 != env_tz_address )
    {
        pstack--; // move to where the TZ environment variable is set TZ=xxx
        *pstack = swap_endian32( env_tz_address );
        tracer.Trace( "the TZ environment argument is at VM address %llx\n", *pstack );
    }

    pstack--; // the last argv is 0 to indicate the end

    for ( int iarg = (int) app_argc - 1; iarg >= 0; iarg-- )
    {
        pstack--;
        *pstack = aargs[ iarg ];
    }

    pstack--;
    *pstack = swap_endian32( app_argc );

    g_top_of_stack = (REG_TYPE) ( ( (uint8_t *) pstack - memory.data() ) + g_base_address );
    REG_TYPE aux_data_size = top_of_aux - (REG_TYPE) ( (uint8_t *) pstack - memory.data() );
    tracer.Trace( "stack at start (beginning with argc) -- %u bytes at address %p:\n", aux_data_size, pstack );
    tracer.TraceBinaryData( (uint8_t *) pstack, (uint32_t) aux_data_size, 2 );

    tracer.Trace( "memory map from highest to lowest addresses:\n" );
    tracer.Trace( "  first byte beyond allocated memory:                 %x\n", g_base_address + memory_size );
    tracer.Trace( "  <mmap arena>                                        (%d = %x bytes)\n", g_mmap_commit, g_mmap_commit );
    tracer.Trace( "  mmap start adddress:                                %x\n", g_base_address + g_mmap_offset );
    tracer.Trace( "  <filler to align to 4k-page for mmap allocations>\n" );

    tracer.Trace( "  <argv data, pointed to by argv array below>         (%d == %x bytes)\n", g_arg_data_commit, g_arg_data_commit );
    tracer.Trace( "  start of argv data:                                 %x\n", g_base_address + arg_data_offset );

    tracer.Trace( "  start of aux data:                                  %x\n", g_top_of_stack + aux_data_size );
    tracer.Trace( "  <random, alignment, aux recs, env, argv>            (%d == %x bytes)\n", aux_data_size, aux_data_size );
    tracer.Trace( "  initial stack pointer g_top_of_stack:               %x\n", g_top_of_stack );
    REG_TYPE stack_bytes = g_stack_commit - aux_data_size;
    tracer.Trace( "  <stack>                                             (%d == %x bytes)\n", stack_bytes, stack_bytes );
    tracer.Trace( "  last byte stack can use (g_bottom_of_stack):        %x\n", g_base_address + g_bottom_of_stack );
    tracer.Trace( "  <unallocated space between brk and the stack>       (%d == %llx bytes)\n", g_brk_commit, g_brk_commit );
    tracer.Trace( "  end_of_data / current brk:                          %x\n", g_base_address + g_end_of_data );
    REG_TYPE uninitialized_bytes = g_end_of_data - first_uninitialized_data;
    tracer.Trace( "  <uninitialized data per the .elf file>              (%d == %x bytes)\n", uninitialized_bytes, uninitialized_bytes );
    tracer.Trace( "  first byte of uninitialized data:                   %x\n", first_uninitialized_data );
    tracer.Trace( "  <initialized data from the .elf file>\n" );
    tracer.Trace( "  <code from the .elf file>\n" );
    tracer.Trace( "  initial pc execution_addess:                        %x\n", g_execution_address );
    tracer.Trace( "  <code per the .elf file>\n" );
    tracer.Trace( "  start of the address space per the .elf file:       %x\n", g_base_address );

    tracer.Trace( "vm memory first byte beyond:     %p\n", memory.data() + memory_size );
    tracer.Trace( "vm memory start:                 %p\n", memory.data() );
    tracer.Trace( "memory_size:                     %#x == %d\n", memory_size, memory_size );

    return true;
} //load_image32

static bool load_68000_hex( const char * pimage )
{
    assert ( 4 == ELF_MACHINE_ISA );
    FILE * fp = fopen( pimage, "rb" );
    if ( !fp )
    {
        printf( "can't open 68000 hex image file: %s\n", pimage );
        usage();
    }

    CFile file( fp );
    char acLine[ 200 ];
    char ac[ 5 ];

    do
    {
        char * buf = fgets( acLine, _countof( acLine ), fp );

        if ( buf && strlen( buf ) >= 8 )
        {
            if ( 'S' != buf[ 0 ] )
                usage( "motorola hex file lines must start with S" );

            if ( '0' == acLine[1] )
            {
                // ignore
            }
            else if ( '1' == acLine[1] )
            {
                ac[ 0 ] = acLine[ 2 ];
                ac[ 1 ] = acLine[ 3 ];
                ac[ 2 ] = 0;
                uint32_t length = strtoul( ac, 0, 16 );
                length -= 3; // 2-byte address at start and 1-byte checksum at end
                ac[ 0 ] = acLine[ 4 ];
                ac[ 1 ] = acLine[ 5 ];
                ac[ 2 ] = acLine[ 6 ];
                ac[ 3 ] = acLine[ 7 ];
                ac[ 4 ] = 0;
                uint32_t address = strtoul( ac, 0, 16 );
                memory.resize( address + length );
                for ( uint32_t i = 0; i < length; i++ )
                {
                    ac[ 0 ] = acLine[ 8 + i * 2 ];
                    ac[ 1 ] = acLine[ 8 + i * 2 + 1 ];
                    ac[ 2 ] = 0;
                    uint8_t v = (uint8_t) strtoul( ac, 0, 16 );
                    memory[ address + i ] = v;
                }
            }
            else if ( '9' == acLine[1] )
            {
                ac[ 0 ] = acLine[ 4 ];
                ac[ 1 ] = acLine[ 5 ];
                ac[ 2 ] = acLine[ 6 ];
                ac[ 3 ] = acLine[ 7 ];
                ac[ 4 ] = 0;
                g_execution_address = strtoul( ac, 0, 16 );
            }
            else
                usage( "motorola hex input file format variation not supported" );
        }
    } while ( !feof( fp ) );

    REG_TYPE memory_size = (REG_TYPE) memory.size();

    if ( memory_size & 0xf )
    {
        memory_size += 16;
        memory_size &= ~0xf;
    }

    g_end_of_data = memory_size;
    g_brk_offset = memory_size;
    g_highwater_brk = memory_size;
    memory_size += g_brk_commit;

    g_bottom_of_stack = memory_size;
    memory_size += g_stack_commit;

    memory_size = round_up( memory_size, (REG_TYPE) 4096 ); // mmap should hand out 4k-aligned pages
    g_mmap_offset = memory_size;
    memory_size += g_mmap_commit;

    memory.resize( memory_size );
    memset( memory.data() + g_brk_offset, 0, memory_size - g_brk_offset );

    g_base_address = 0;
    g_mmap.initialize( g_base_address + g_mmap_offset, g_mmap_commit, memory.data() - g_base_address );

    g_top_of_stack = (REG_TYPE) memory.size();

    tracer.Trace( "memory map from highest to lowest addresses:\n" );
    tracer.Trace( "  first byte beyond allocated memory:                 %x\n", g_base_address + memory_size );
    tracer.Trace( "  <mmap arena>                                        (%d = %x bytes)\n", g_mmap_commit, g_mmap_commit );
    tracer.Trace( "  mmap start adddress:                                %x\n", g_base_address + g_mmap_offset );
    tracer.Trace( "  <align to 4k-page for mmap allocations>\n" );
    tracer.Trace( "  initial stack pointer g_top_of_stack:               %x\n", g_top_of_stack );
    REG_TYPE stack_bytes = g_stack_commit;
    tracer.Trace( "  <stack>                                             (%d == %x bytes)\n", stack_bytes, stack_bytes );
    tracer.Trace( "  last byte stack can use (g_bottom_of_stack):        %x\n", g_base_address + g_bottom_of_stack );
    tracer.Trace( "  <unallocated space between brk and the stack>       (%d == %llx bytes)\n", g_brk_commit, g_brk_commit );
    tracer.Trace( "  end_of_data / current brk:                          %x\n", g_base_address + g_end_of_data );
    tracer.Trace( "  <code + data from the .hex file>\n" );
    tracer.Trace( "  initial pc execution_addess:                        %x\n", g_execution_address );
    tracer.Trace( "  <code per the .hex file>\n" );
    tracer.Trace( "  start of the address space:                         %x\n", g_base_address );

    tracer.Trace( "vm memory first byte beyond:     %p\n", memory.data() + memory_size );
    tracer.Trace( "vm memory start:                 %p\n", memory.data() );
    tracer.Trace( "memory_size:                     %#x == %d\n", memory_size, memory_size );

    return true;
} //load_68000_hex

#endif

static bool load_image( const char * pimage, const char * app_args )
{
    tracer.Trace( "loading image %s\n", pimage );

#ifdef M68
    if ( ends_with( pimage, ".hex" ) ) // Motorola 68000 hex file special-case
        return load_68000_hex( pimage ); // app args are lost with hex files

    if ( ends_with( pimage, ".68k" ) ) // Digital Research CP/M 68K executable file
        return load_cpm68k( pimage, app_args );
#endif

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

    if ( 0x464c457f != ehead.magic && 0x7f454c46 != ehead.magic )
        usage( "elf image file's magic header is invalid" );

#ifdef M68
    if ( 1 == ehead.bit_width )
        return load_image32( fp, pimage, app_args );
#endif

#ifndef M68

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
    g_execution_address = (REG_TYPE) ehead.entry_point;
    g_compressed_rvc = 0 != ( ehead.flags & 1 ); // 2-byte compressed RVC instructions, not 4-byte default risc-v instructions

    // determine how much RAM to allocate

    REG_TYPE memory_size = 0;

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
            g_base_address = (REG_TYPE) head.physical_address;
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

    // use known qsort so traces are consistent across platforms because qsort implementations for ties differ
    my_qsort( g_symbols.data(), g_symbols.size(), sizeof( ElfSymbol64 ), symbol_compare );

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
    //     uninitalized data bss (size read from the .elf file)
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
    memory_size += g_arg_data_commit;
    g_end_of_data = memory_size;
    g_brk_offset = memory_size;
    g_highwater_brk = memory_size;
    memory_size += g_brk_commit;

    g_bottom_of_stack = memory_size;
    memory_size += g_stack_commit;

    uint64_t top_of_aux = memory_size;
    memory_size = round_up( memory_size, (REG_TYPE) 4096 ); // mmap should hand out 4k-aligned pages
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

    const uint32_t max_args = 40;
    REG_TYPE aargs[ max_args ]; // vm pointers to each arguments
    char * buffer_args = (char *) ( memory.data() + arg_data_offset );
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
        aargs[ app_argc ] = offset + g_base_address + arg_data_offset;
        tracer.Trace( "  argument %d is '%s', at vm address %llx\n", app_argc, pargs, (uint64_t) offset + g_base_address + arg_data_offset );

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
    tracer.TraceBinaryData( (uint8_t *) ( memory.data() + arg_data_offset ), g_arg_data_commit + 0x20, 4 ); // +20 to inspect for bugs

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
        tracer.Trace( "the TZ environment argument is at VM address %llx\n", env_tz_address );
    }

    pstack--; // the last argv is 0 to indicate the end

    for ( int iarg = (int) app_argc - 1; iarg >= 0; iarg-- )
    {
        pstack--;
        *pstack = aargs[ iarg ];
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

#endif

    return true;
} //load_image

static void elf_info32( FILE * fp, bool verbose )
{
    ElfHeader32 ehead = {0};
    fseek( fp, (long) 0, SEEK_SET );
    size_t read = fread( &ehead, 1, sizeof ehead, fp );

    if ( 0x464c457f != ehead.magic && 0x7f454c46 != ehead.magic )
    {
        printf( "image file's magic header is invalid: %x\n", ehead.magic );
        return;
    }

    bool big_endian = ( 2 == ehead.endianness );
    printf( "image is %s endian\n", big_endian ? "big" : "little" );

    if ( big_endian)
        ehead.swap_endianness();

    if ( ELF_MACHINE_ISA != ehead.machine )
        printf( "image machine ISA isn't a match for %s; continuing anyway. machine type is %x\n", APP_NAME, ehead.machine );

    printf( "header fields:\n" );
    printf( "  bit_width: %u\n", ehead.bit_width );
    printf( "  type: %u\n", ehead.type );
    printf( "  entry address: %#x\n", ehead.entry_point );
    printf( "  program entries: %u\n", ehead.program_header_table_entries );
    printf( "  program header entry size: %u\n", ehead.program_header_table_size );
    printf( "  program offset: %u == %x\n", ehead.program_header_table, ehead.program_header_table );
    printf( "  section entries: %u\n", ehead.section_header_table_entries );
    printf( "  section header entry size: %u\n", ehead.section_header_table_size );
    printf( "  section offset: %u == %#x\n", ehead.section_header_table, ehead.section_header_table );
    printf( "  flags: %#x\n", ehead.flags );

    g_execution_address = ehead.entry_point;
    REG_TYPE memory_size = 0;

    printf( "program headers:\n" );
    printf( "   # Type       Offset   VirtAddr PhysAddr FileSize MemSize  Alignment Flags\n" );

    for ( uint16_t ph = 0; ph < ehead.program_header_table_entries; ph++ )
    {
        size_t o = ehead.program_header_table + ( ph * ehead.program_header_table_size );
        ElfProgramHeader32 head = {0};

        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, 1, get_min( sizeof( head ), (size_t) ehead.program_header_table_size ), fp );
        if ( 0 == read )
            usage( "can't read program header" );

        if ( big_endian)
            head.swap_endianness();

        printf( "  %2u", ph );
        printf( " %-10s", head.show_type() );
        printf( " %08x", head.offset_in_image );
        printf( " %08x", head.virtual_address );
        printf( " %08x", head.physical_address );
        printf( " %08x", head.file_size );
        printf( " %08x", head.memory_size );
        printf( " %08x", head.alignment );
        printf( "  %s\n", head.show_flags() );

        REG_TYPE just_past = head.physical_address + head.memory_size;
        if ( just_past > memory_size )
            memory_size = just_past;

        if ( 0 != head.physical_address )
        {
            if ( ( 0 == g_base_address ) || ( g_base_address > head.physical_address ) )
                g_base_address = head.physical_address;
        }
    }

    memory_size -= g_base_address;

    // first load the string tables

    vector<char> string_table;
    vector<char> shstr_table;

    for ( uint16_t sh = 0; sh < ehead.section_header_table_entries; sh++ )
    {
        size_t o = ehead.section_header_table + ( sh * ehead.section_header_table_size );
        ElfSectionHeader32 head = {0};

        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, 1, get_min( sizeof( head ), (size_t) ehead.section_header_table_size ), fp );
        if ( 0 == read )
            usage( "can't read section header" );

        if ( big_endian)
            head.swap_endianness();

        if ( 3 == head.type )
        {
            if ( 0 == string_table.size() )
            {
                string_table.resize( head.size );
                fseek( fp, (long) head.offset, SEEK_SET );
                read = fread( string_table.data(), 1, head.size, fp );
                if ( 0 == read )
                    usage( "can't read string table\n" );
            }
            else
            {
                shstr_table.resize( head.size );
                fseek( fp, (long) head.offset, SEEK_SET );
                read = fread( shstr_table.data(), 1, head.size, fp );
                if ( 0 == read )
                    usage( "can't read shstr table\n" );
            }
        }
    }

    printf( "section headers:\n" );
    printf( "   # Name                 Type                             Address  Offset   Size     Flags\n" );

    for ( uint16_t sh = 0; sh < ehead.section_header_table_entries; sh++ )
    {
        size_t o = ehead.section_header_table + ( sh * ehead.section_header_table_size );
        //printf( "section header %u at offset %zu == %zx\n", sh, o, o );

        ElfSectionHeader32 head = {0};

        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, 1, get_min( sizeof( head ), (size_t) ehead.section_header_table_size ), fp );
        if ( 0 == read )
            usage( "can't read section header" );

        if ( big_endian)
            head.swap_endianness();

        printf( "  %2u", sh );
        printf( " %-20s", & shstr_table[ head.name_offset ] );
        printf( " %-32s", head.show_type() );
        printf( " %08x", head.address );
        printf( " %08x", head.offset );
        printf( " %08x", head.size );
        printf( " %#x / %s\n", head.flags, head.show_flags() );

        if ( 2 == head.type ) // symbol table
        {
            vector<ElfSymbol32> symbols;
            symbols.resize( head.size / sizeof( ElfSymbol32 ) );
            fseek( fp, (long) head.offset, SEEK_SET );
            read = fread( symbols.data(), 1, head.size, fp );
            if ( 0 == read )
                usage( "can't read symbol table\n" );

            if ( verbose )
            {
                size_t count = head.size / sizeof( ElfSymbol32 );
                printf( "  symbols:\n" );
                for ( size_t sym = 0; sym < symbols.size(); sym++ )
                {
                    printf( "    symbol # %zd\n", sym );
                    ElfSymbol32 & sym_entry = symbols[ sym ];
                    if ( big_endian)
                        sym_entry.swap_endianness();

                    printf( "     name:  %x == %s\n",   sym_entry.name, ( 0 == sym_entry.name ) ? "" : & string_table[ sym_entry.name ] );
                    printf( "     info:  %x == %s\n",   sym_entry.info, sym_entry.show_info() );
                    printf( "     other: %x == %s\n",   sym_entry.other, sym_entry.show_other() );
                    printf( "     shndx: %x\n",   sym_entry.shndx );
                    printf( "     value: %x\n", sym_entry.value );
                    printf( "     size:  %u\n", sym_entry.size );
                }
            }
        }
        else if ( 7 == head.type && 0 != head.size && verbose ) // notes
        {
            vector<uint8_t> notes( head.size );
            fseek( fp, (long) head.offset, SEEK_SET );
            read = fread( notes.data(), 1, head.size, fp );
            if ( 0 == read )
                usage( "can't read notes\n" );
            tracer.PrintBinaryData( notes.data(), (uint32_t) head.size, 4 );
        }
    }

    printf( "global info\n" );
    printf( "  flags: %#08x\n", ehead.flags );

    printf( "  vm g_base_address %llx\n", (uint64_t) g_base_address );
    printf( "  memory_size: %llx\n", (uint64_t) memory_size );
    printf( "  g_stack_commit: %llx\n", (uint64_t) g_stack_commit );
    printf( "  g_execution_address %llx\n", (uint64_t) g_execution_address );
} //elf_info32

static void elf_info( const char * pimage, bool verbose )
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

    if ( 1 == ehead.bit_width )
    {
        elf_info32( fp, verbose );
        return;
    }

#ifndef M68

    if ( 0x464c457f != ehead.magic && 0x7f454c46 != ehead.magic )
    {
        printf( "image file's magic header is invalid: %x\n", ehead.magic );
        return;
    }

    if ( 1 != ehead.endianness )
    {
        printf( "expected a little-endian image\n" );
        return;
    }

    if ( ELF_MACHINE_ISA != ehead.machine )
        printf( "image machine ISA isn't a match for %s; continuing anyway. machine type is %x\n", APP_NAME, ehead.machine );

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
    REG_TYPE memory_size = 0;

    printf( "program headers:\n" );
    printf( "   # Type       Offset   VirtAddr PhysAddr FileSize MemSize  Alignment Flags\n" );

    for ( uint16_t ph = 0; ph < ehead.program_header_table_entries; ph++ )
    {
        size_t o = ehead.program_header_table + ( ph * ehead.program_header_table_size );
        ElfProgramHeader64 head = {0};

        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, 1, get_min( sizeof( head ), (size_t) ehead.program_header_table_size ), fp );
        if ( 0 == read )
            usage( "can't read program header" );

        printf( "  %2u", ph );
        printf( " %-10s", head.show_type() );
        printf( " %08llx", head.offset_in_image );
        printf( " %08llx", head.virtual_address );
        printf( " %08llx", head.physical_address );
        printf( " %08llx", head.file_size );
        printf( " %08llx", head.memory_size );
        printf( " %08llx", head.alignment );
        printf( "  %s\n", head.show_flags() );

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

    // first load the string tables

    vector<char> string_table;
    vector<char> shstr_table;

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
            if ( 0 == string_table.size() )
            {
                string_table.resize( head.size );
                fseek( fp, (long) head.offset, SEEK_SET );
                read = fread( string_table.data(), 1, head.size, fp );
                if ( 0 == read )
                    usage( "can't read string table\n" );
            }
            else
            {
                shstr_table.resize( head.size );
                fseek( fp, (long) head.offset, SEEK_SET );
                read = fread( shstr_table.data(), 1, head.size, fp );
                if ( 0 == read )
                    usage( "can't read shstr table\n" );
            }
        }
    }

    printf( "section headers:\n" );
    printf( "   # Name                 Type                             Address  Offset   Size     Flags\n" );

    for ( uint16_t sh = 0; sh < ehead.section_header_table_entries; sh++ )
    {
        size_t o = ehead.section_header_table + ( sh * ehead.section_header_table_size );
        //printf( "section header %u at offset %zu == %zx\n", sh, o, o );

        ElfSectionHeader64 head = {0};

        fseek( fp, (long) o, SEEK_SET );
        read = fread( &head, 1, get_min( sizeof( head ), (size_t) ehead.section_header_table_size ), fp );
        if ( 0 == read )
            usage( "can't read section header" );

        printf( "  %2u", sh );
        printf( " %-20s", & shstr_table[ head.name_offset ] );
        printf( " %-32s", head.show_type() );
        printf( " %08llx", head.address );
        printf( " %08llx", head.offset );
        printf( " %08llx", head.size );
        printf( " %#llx / %s\n", head.flags, head.show_flags() );

        if ( 2 == head.type ) // symbol table
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
        else if ( 7 == head.type && 0 != head.size && verbose ) // notes
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

    printf( "global info\n" );
    printf( "  flags: %#08x\n", ehead.flags );
    printf( "    contains 2-byte compressed RVC instructions: %s\n", g_compressed_rvc ? "yes" : "no" );
    printf( "    contains 4-byte float instructions: %s\n", ( ehead.flags & 2 ) ? "yes" : "no" );
    printf( "    contains 8-byte double instructions: %s\n", ( ehead.flags & 4 ) ? "yes" : "no" );
    printf( "    RV TSO memory consistency: %s\n", ( ehead.flags & 0x10 ) ? "yes" : "no" );
    printf( "    contains non-standard extensions: %s\n", ( ehead.flags & 0xff000000 ) ? "yes" : "no" );

    printf( "  vm g_base_address %llx\n", (uint64_t) g_base_address );
    printf( "  memory_size: %llx\n", (uint64_t) memory_size );
    printf( "  g_stack_commit: %llx\n", (uint64_t) g_stack_commit );
    printf( "  g_execution_address %llx\n", (uint64_t) g_execution_address );
#endif
} //elf_info

int main( int argc, char * argv[] )
{
    try
    {
        uint16_t tst = 1;
        g_hostIsLittleEndian = ( 1 & ( * (uint8_t *) &tst ) );

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

                    REG_TYPE heap = (REG_TYPE) strtoull( parg + 3 , 0, 10 );
                    if ( heap > 1024 ) // limit to a gig
                        usage( "invalid heap size specified" );

                    g_brk_commit = heap * 1024 * 1024;
                }
                else if ( 'm' == ca )
                {
                    if ( ':' != parg[2] )
                        usage( "the -m argument requires a value" );

                    REG_TYPE mmap_space = (REG_TYPE) strtoull( parg + 3 , 0, 10 );
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
            high_resolution_clock::time_point tStart = high_resolution_clock::now();

            #ifdef _WIN32
                g_tAppStart = tStart;
            #endif

            uint64_t cycles = cpu->run();

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
        printf( "caught exception bad_alloc -- out of RAM. If in RVOS/ARMOS/M68 use -h or -m to add RAM. %s\n", e.what() );
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
