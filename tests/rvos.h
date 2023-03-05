#pragma once

#define rvos_sys_exit 0x1
#define rvos_sys_print_text 0x2
#define rvos_sys_rand 0x2000
#define rvos_sys_print_double 0x2001
#define rvos_sys_trace_instructions 0x2002

extern "C" void riscv_printf( const char * fmt, ... );
extern "C" int riscv_sprintf( char * pc, const char * fmt, ... );
extern "C" void riscv_print_text( const char * pc );
extern "C" uint64_t riscv_rand( void );
extern "C" uint64_t riscv_exit( int code );
extern "C" char * riscv_floattoa( char *buffer, float f, int precision );
extern "C" void riscv_print_double( double d );
extern "C" int riscv_gettimeofday( struct timeval * p, void * x );
extern "C" bool riscv_trace_instructions( bool enable );
