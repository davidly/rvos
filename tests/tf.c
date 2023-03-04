#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include "rvos.h"

// doubles don't work. it may be my bug or a bug in the gnu's double emulation
char *ftoa( char *buffer, double d, int precision )
{
    riscv_print_text( "top of ftoa, d: \n" );
    riscv_print_double( d );
    riscv_print_text( "\n" );
    char * pbuf = buffer;

    if ( d < 0 )
    {
        riscv_print_text( "d is negative\n" );
        *pbuf++ = '-';
        *pbuf = 0;
        d *= -1;
    }

    uint32_t whole = (uint32_t) (float) d;
    riscv_printf( "in ftoa, whole %u\n", whole );

    itoa( whole, pbuf, 10 );

    if ( precision > 0 )
    {
        riscv_printf( "precision is > 0\n" );
        pbuf = buffer + strlen( buffer );
        *pbuf++ = '.';

        double fraction = d - whole;

        while ( precision > 0 )
        {
            riscv_printf( "precision while loop, precision = %d, fraction: \n", precision );
    riscv_print_double( fraction );
    riscv_print_text( "\n" );

            fraction *= 10.0;
            whole = fraction;
            *pbuf++ = '0' + whole;

            fraction -= whole;
            precision--;
        }

        *pbuf = 0;
    }

    return buffer;
} //ftoa

#pragma GCC optimize ("O0")
extern "C" int main()
{
    char ac[ 100 ];

    printf( "hello from printf\n" );

    riscv_floattoa( ac, -1.234567, 8 );
    riscv_printf( "float converted by floattoa: %s\n", ac );
    riscv_floattoa( ac, 1.234567, 8 );
    riscv_printf( "float converted by floattoa: %s\n", ac );
    riscv_floattoa( ac, 34.567, 8 );
    riscv_printf( "float converted by floattoa: %s\n", ac );

    riscv_printf( "float from printf: %f\n", 45.678 );

    float f1 = 1.0;
    float f2 = 20.2;
    float fm1 = -1.342;
    float fr = f2 * fm1;
    float fd = 1000.0 / 3.0;
    float fs = sqrt( fd );

    riscv_printf( "division result: %f, square root %f\n", fd, fs );

    // The tool chain is broken and only supports hardware floats and software doubles.

    riscv_print_text( "calling floattoa\n" );
    riscv_floattoa( ac, fr, 6 );
    riscv_printf( "float converted with riscv_floattoa: %s\n", ac );

    riscv_printf( "result of 20.2 * -1.342: %f\n", fr );

    double d = (double) fr;
    riscv_print_text( "now printing fr as a double:\n" );
    riscv_printf( "result of 20.2 * -1.342 as a double: %lf\n", d );

    riscv_print_text( "stop\n" );
    exit( 0 );
} //main


