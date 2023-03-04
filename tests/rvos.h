#pragma once

extern "C" void riscv_printf( const char * fmt, ... );
extern "C" int riscv_sprintf( char * pc, const char * fmt, ... );
extern "C" void riscv_print_text( const char * pc );
extern "C" uint64_t riscv_rand( void );
extern "C" uint64_t riscv_exit( int code );
extern "C" char * riscv_floattoa( char *buffer, float f, int precision );
extern "C" void riscv_print_double( double d );
extern "C" int riscv_gettimeofday( struct timeval * p, void * x );
