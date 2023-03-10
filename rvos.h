#pragma once

#define rvos_sys_rand               0x2000
#define rvos_sys_print_double       0x2001
#define rvos_sys_trace_instructions 0x2002
#define rvos_sys_exit               0x2003
#define rvos_sys_print_text         0x2004

// Linux syscall numbers differ by ISA. InSAne. These are RISC
// https://marcin.juszkiewicz.com.pl/download/tables/syscalls.html

#define SYS_mkdirat 34
#define SYS_unlinkat 35
#define SYS_chdir 49
#define SYS_openat 56
#define SYS_close 57
#define SYS_lseek 62
#define SYS_read 63
#define SYS_write 64
#define SYS_writev 66
#define SYS_readlinkat 78
#define SYS_newfstat 79
#define SYS_fstat 80
#define SYS_fdatasync 83
#define SYS_exit 93
#define SYS_exit_group 94
#define SYS_set_tid_address 96
#define SYS_futex 98
#define SYS_set_robust_list 99
#define SYS_clock_gettime 113
#define SYS_tgkill 131
#define SYS_rt_sigprocmask 135
#define SYS_gettimeofday 169
#define SYS_getpid 172
#define SYS_gettid 178
#define SYS_sysinfo 179
#define SYS_brk 214
#define SYS_mmap 222
#define SYS_mprotect 226
#define SYS_riscv_flush_icache 259
#define SYS_prlimit64 261
#define SYS_getrandom 278

// open apparently undefined for riscv? the old g++ compiler/runtime uses this value

#define SYS_open 1024

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

