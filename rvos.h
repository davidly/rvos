#pragma once

#define rvos_sys_rand               0x2000
#define rvos_sys_print_double       0x2001
#define rvos_sys_trace_instructions 0x2002
#define rvos_sys_exit               0x2003
#define rvos_sys_print_text         0x2004

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

extern "C" void rvos_printf( const char * fmt, ... );
extern "C" int rvos_sprintf( char * pc, const char * fmt, ... );
extern "C" void rvos_print_text( const char * pc );
extern "C" uint64_t rvos_rand( void );
extern "C" uint64_t rvos_exit( int code );
extern "C" char * rvos_floattoa( char *buffer, float f, int precision );
extern "C" void rvos_print_double( double d );
extern "C" int rvos_gettimeofday( struct timeval * p, void * x );
extern "C" bool rvos_trace_instructions( bool enable );

