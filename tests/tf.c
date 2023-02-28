#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

extern "C" void riscv_print_text( const char * p );

#if defined( _MSC_VER ) || defined(WIN64)

extern "C" void riscv_print_text( const char * p )
{
    printf( "%s", p );
}
#endif

#pragma GCC optimize ("O0")
extern "C" int main()
{
    float f1 = 1.0;
    float f2 = 2.0;
    float fm1 = -1.0;
    float fr = f2 * fm1;

    riscv_print_text( "stop\n" );
    return 0;
} //main


