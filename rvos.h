#pragma once

#define rvos_sys_rand               0x2000
#define rvos_sys_print_double       0x2001
#define rvos_sys_trace_instructions 0x2002
#define rvos_sys_exit               0x2003
#define rvos_sys_print_text         0x2004
#define rvos_sys_get_datetime       0x2005

// Linux syscall numbers differ by ISA. InSAne. These are RISC
// Note that there are differences between these two sets. which is correct?
// https://marcin.juszkiewicz.com.pl/download/tables/syscalls.html
// https://github.com/westerndigitalcorporation/RISC-V-Linux/blob/master/linux/arch/s390/kernel/syscalls/syscall.tbl

#define SYS_getcwd 17
#define SYS_ioctl 29
#define SYS_mkdirat 34
#define SYS_unlinkat 35
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
#define SYS_prctl 167
#define SYS_gettimeofday 169
#define SYS_getpid 172
#define SYS_gettid 178
#define SYS_sysinfo 179
#define SYS_brk 214
#define SYS_munmap 215
#define SYS_mremap 216
#define SYS_clone 220
#define SYS_mmap 222
#define SYS_mprotect 226
#define SYS_riscv_flush_icache 259
#define SYS_prlimit64 261
#define SYS_renameat2 276
#define SYS_getrandom 278

// open apparently undefined for riscv? the old g++ compiler/runtime uses this value

#define SYS_open 1024
#define SYS_link 1025
#define SYS_unlink 1026
#define SYS_mkdir 1030
#define SYS_stat 1038
#define SYS_lstat 1039
#define SYS_time 1062

extern "C" void rvos_printf( const char * fmt, ... );
extern "C" int rvos_sprintf( char * pc, const char * fmt, ... );
extern "C" void rvos_print_text( const char * pc );
extern "C" uint64_t rvos_rand( void );
extern "C" uint64_t rvos_exit( int code );
extern "C" char * rvos_floattoa( char *buffer, float f, int precision );
extern "C" void rvos_print_double( double d );
extern "C" int rvos_gettimeofday( struct timeval * p, void * x );
extern "C" bool rvos_trace_instructions( bool enable );
extern "C" void rvos_sp_add( uint64_t val );

