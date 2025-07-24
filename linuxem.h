#pragma once

// the emulator only supports paths and filenames up to this length across all platforms

#define EMULATOR_MAX_PATH 2048
#define EMULATOR_AT_SYMLINK_NOFOLLOW 0x100
#define EMULATOR_AT_REMOVEDIR 0x200 // this is 8 for newlib

#define emulator_sys_rand               0x2000
#define emulator_sys_print_double       0x2001
#define emulator_sys_trace_instructions 0x2002
#define emulator_sys_exit               0x2003
#define emulator_sys_print_text         0x2004
#define emulator_sys_get_datetime       0x2005
#define emulator_sys_print_int64        0x2006
#define emulator_sys_print_char         0x2007

// Linux syscall numbers differ by ISA. InSAne. These are RISC and ARM64, which are the same!
// Note that there are differences between these two sets. which is correct?
// https://marcin.juszkiewicz.com.pl/download/tables/syscalls.html
// https://github.com/westerndigitalcorporation/RISC-V-Linux/blob/master/linux/arch/s390/kernel/syscalls/syscall.tbl
// https://blog.xhyeax.com/2022/04/28/arm64-syscall-table/
// https://syscalls.mebeim.net/?table=arm64/64/aarch64/latest

#define SYS_getcwd 17
#define SYS_fcntl 25
#define SYS_ioctl 29
#define SYS_mkdirat 34
#define SYS_unlinkat 35
#define SYS_renameat 38
#define SYS_faccessat 48
#define SYS_chdir 49
#define SYS_openat 56
#define SYS_close 57
#define SYS_getdents64 61
#define SYS_lseek 62
#define SYS_read 63
#define SYS_write 64
#define SYS_writev 66
#define SYS_pselect6 72   // or sigsuspend?
#define SYS_ppoll_time32 73
#define SYS_readlinkat 78
#define SYS_newfstatat 79
#define SYS_newfstat 80
#define SYS_fsync 82
#define SYS_fdatasync 83
#define SYS_exit 93
#define SYS_exit_group 94
#define SYS_set_tid_address 96
#define SYS_futex 98
#define SYS_set_robust_list 99
#define SYS_clock_gettime 113
#define SYS_clock_nanosleep 115
#define SYS_sched_setaffinity 122
#define SYS_sched_getaffinity 123
#define SYS_sched_yield 124
#define SYS_tgkill 131
#define SYS_signalstack 132
#define SYS_sigaction 134
#define SYS_rt_sigprocmask 135
#define SYS_times 153
#define SYS_uname 160
#define SYS_getrusage 165
#define SYS_prctl 167
#define SYS_gettimeofday 169
#define SYS_getpid 172
#define SYS_getuid 174
#define SYS_geteuid 175
#define SYS_getgid 176
#define SYS_getegid 177
#define SYS_gettid 178
#define SYS_sysinfo 179
#define SYS_brk 214
#define SYS_munmap 215
#define SYS_mremap 216
#define SYS_clone 220
#define SYS_mmap 222
#define SYS_mprotect 226
#define SYS_madvise 233
#define SYS_riscv_flush_icache 259 // not in docs; may be riscv only
#define SYS_prlimit64 261
#define SYS_renameat2 276
#define SYS_getrandom 278
#define SYS_statx 291
#define SYS_rseq 293

// open apparently undefined for riscv? the old RISC-V64 g++ compiler/runtime uses these syscalls

#define SYS_open 1024
#define SYS_link 1025
#define SYS_unlink 1026
#define SYS_mkdir 1030
#define SYS_stat 1038
#define SYS_lstat 1039
#define SYS_time 1062

// structs passed to syscalls.

extern uint64_t swap_endian64( uint64_t x );
extern uint32_t swap_endian32( uint32_t x );
extern uint16_t swap_endian16( uint16_t x );

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

struct timespec_syscall
{
    uint64_t tv_sec;
    uint64_t tv_nsec;

    void swap_endianness()
    {
        tv_sec = swap_endian64( tv_sec );
        tv_nsec = swap_endian64( tv_nsec );
    }
};

struct linux_timeval_syscall
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

struct linux_tms_syscall32
{
    uint32_t tms_utime;
    uint32_t tms_stime;
    uint32_t tms_cutime;
    uint32_t tms_cstime;
};

struct stat_linux_syscall
{
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
         120         8 st_mystery_spot_2
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
    uint64_t   st_blksize;  /* Block size for filesystem I/O */
    uint64_t   st_blocks;   /* Number of 512 B blocks allocated */

    /* Since POSIX.1-2008, this structure supports nanosecond
       precision for the following timestamp fields.
       For the details before POSIX.1-2008, see VERSIONS. */

    struct timespec_syscall st_atim;  /* Time of last access */
    struct timespec_syscall st_mtim;  /* Time of last modification */
    struct timespec_syscall st_ctim;  /* Time of last status change */

    uint64_t   st_mystery_spot_2;

    void swap_endianness()
    {
        st_dev = swap_endian64( st_dev );
        st_ino = swap_endian64( st_ino );
        st_mode = swap_endian32( st_mode );
        st_nlink = swap_endian32( st_nlink );
        st_uid = swap_endian32( st_uid );
        st_gid = swap_endian32( st_gid );
        st_rdev = swap_endian64( st_rdev );
        st_mystery_spot = swap_endian64( st_mystery_spot );
        st_size = swap_endian64( st_size );
        st_blksize = swap_endian64( st_blksize );
        st_blocks = swap_endian64( st_blocks );
        st_atim.swap_endianness();
        st_mtim.swap_endianness();
        st_ctim.swap_endianness();
    }
};

#ifndef STATX_TYPE
# define STATX_TYPE 0x0001U
# define STATX_MODE 0x0002U
# define STATX_NLINK 0x0004U
# define STATX_UID 0x0008U
# define STATX_GID 0x0010U
# define STATX_ATIME 0x0020U
# define STATX_MTIME 0x0040U
# define STATX_CTIME 0x0080U
# define STATX_INO 0x0100U
# define STATX_SIZE 0x0200U
# define STATX_BLOCKS 0x0400U
# define STATX_BASIC_STATS 0x07ffU
# define STATX_ALL 0x0fffU
# define STATX_BTIME 0x0800U
# define STATX_MNT_ID 0x1000U
# define STATX_DIOALIGN 0x2000U
# define STATX__RESERVED 0x80000000U
#endif

#pragma pack( push, 1 )
struct statx_timestamp_linux_syscall
{
    int64_t tv_sec;     /* Seconds since the Epoch (UNIX time) */
    uint32_t tv_nsec;   /* Nanoseconds since tv_sec */
    void swap_endianness()
    {
        tv_sec = swap_endian64( tv_sec );
        tv_nsec = swap_endian32( tv_nsec );
    }
};

struct statx_linux_syscall
{
    uint32_t stx_mask;        /* Mask of bits indicating filled fields */
    uint32_t stx_blksize;     /* Block size for filesystem I/O */
    uint64_t stx_attributes;  /* Extra file attribute indicators */
    uint32_t stx_nlink;       /* Number of hard links */
    uint32_t stx_uid;         /* User ID of owner */
    uint32_t stx_gid;         /* Group ID of owner */
    uint16_t stx_mode;        /* File type and mode */
    uint64_t stx_ino;         /* Inode number */
    uint64_t stx_size;        /* Total size in bytes */
    uint64_t stx_blocks;      /* Number of 512B blocks allocated */
    uint64_t stx_attributes_mask; /* Mask to show what's supported in stx_attributes */

    /* The following fields are file timestamps */
    struct statx_timestamp_linux_syscall stx_atime;  /* Last access */
    struct statx_timestamp_linux_syscall stx_btime;  /* Creation */
    struct statx_timestamp_linux_syscall stx_ctime;  /* Last status change */
    struct statx_timestamp_linux_syscall stx_mtime;  /* Last modification */

    /* If this file represents a device, then the next two fields contain the ID of the device */
    uint32_t stx_rdev_major;  /* Major ID */
    uint32_t stx_rdev_minor;  /* Minor ID */

    /* The next two fields contain the ID of the device containing the filesystem where the file resides */
    uint32_t stx_dev_major;   /* Major ID */
    uint32_t stx_dev_minor;   /* Minor ID */

    uint64_t stx_mnt_id;      /* Mount ID */

    /* Direct I/O alignment restrictions */
    uint32_t stx_dio_mem_align;
    uint32_t stx_dio_offset_align;

    uint64_t stx_subvol;      /* Subvolume identifier */

    /* Direct I/O atomic write limits */
    uint32_t stx_atomic_write_unit_min;
    uint32_t stx_atomic_write_unit_max;
    uint32_t stx_atomic_write_segments_max;

    /* File offset alignment for direct I/O reads */
    uint32_t   stx_dio_read_offset_align;

    void swap_endianness()
    {
        stx_mask = swap_endian32( stx_mask );
        stx_blksize = swap_endian32( stx_blksize );
        stx_attributes = swap_endian64( stx_attributes );
        stx_nlink = swap_endian32( stx_nlink );
        stx_uid = swap_endian32( stx_uid );
        stx_gid = swap_endian32( stx_gid );
        stx_mode = swap_endian16( stx_mode );
        stx_ino = swap_endian64( stx_ino );
        stx_size = swap_endian64( stx_size );
        stx_blocks = swap_endian64( stx_blocks );
        stx_atime.swap_endianness();
        stx_mtime.swap_endianness();
        stx_ctime.swap_endianness();
    }
};
#pragma pack(pop)

#pragma warning(disable: 4200) // 0-sized array
struct linux_dirent64_syscall
{
    uint64_t d_ino;     /* Inode number */
    uint64_t d_off;     /* Offset to next linux_dirent */
    uint16_t d_reclen;  /* Length of this linux_dirent */
    uint8_t  d_type;    /* DT_DIR (4) if a dir, DT_REG (8) if a regular file */
    char     d_name[];
    /* optional and not implemented. must be 0-filled
    char pad
    char d_type
    */

    void swap_endianness()
    {
        d_ino = swap_endian64( d_ino );
        d_off = swap_endian64( d_off );
        d_reclen = swap_endian16( d_reclen );
    }
};

struct linux_rusage_syscall
{
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

struct linux_rusage_syscall32
{
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

struct pollfd_syscall
{
    int fd;
    short events;
    short revents;
};

#define SYS_NMLN 65 // appears to be true for Arm64.

struct utsname_syscall
{
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

#define local_KERNEL_NCCS 19

struct local_kernel_termios
{
    uint32_t c_iflag;     /* input mode flags */
    uint32_t c_oflag;     /* output mode flags */
    uint32_t c_cflag;     /* control mode flags */
    uint32_t c_lflag;     /* local mode flags */
    uint8_t c_line;       /* line discipline */
    uint8_t c_cc[local_KERNEL_NCCS]; /* control characters */

    void swap_endianness()
    {
        c_iflag = swap_endian32( c_iflag );
        c_oflag = swap_endian32( c_oflag );
        c_cflag = swap_endian32( c_cflag );
        c_lflag = swap_endian32( c_lflag );
    }
};

#define AT_EH_FRAME_BEGIN 0x69690069 // address of __EH_FRAME_BEGIN__  Not a real constant; just for emulators

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

        // commented out because these need to be 32 bit quantities on all platforms
        //void * a_ptr;
        //void ( * a_fcn )();
    } a_un;

    void swap_endianness()
    {
        a_type = swap_endian32( a_type );
        a_un.a_val = swap_endian32( a_un.a_val );
    }
};


