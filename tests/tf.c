#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <stdint.h>

#if 0
char *ftoa( char *buffer, double d, int precision )
{
    rvos_print_text( "top of ftoa, d: \n" );
    rvos_print_double( d );
    rvos_print_text( "\n" );
    char * pbuf = buffer;

    if ( d < 0 )
    {
        rvos_print_text( "d is negative\n" );
        *pbuf++ = '-';
        *pbuf = 0;
        d *= -1;
    }

    uint32_t whole = (uint32_t) (float) d;
    rvos_printf( "in ftoa, whole %u\n", whole );

    itoa( whole, pbuf, 10 );

    if ( precision > 0 )
    {
        rvos_printf( "precision is > 0\n" );
        pbuf = buffer + strlen( buffer );
        *pbuf++ = '.';

        double fraction = d - whole;

        while ( precision > 0 )
        {
            rvos_printf( "precision while loop, precision = %d, fraction: \n", precision );
    rvos_print_double( fraction );
    rvos_print_text( "\n" );

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
#endif

#pragma GCC optimize ("O0")
extern "C" int main()
{
    char ac[ 100 ];

    printf( "hello from printf\n" );

#if false
    rvos_floattoa( ac, -1.234567, 8 );
    rvos_printf( "float converted by floattoa: %s\n", ac );
    rvos_floattoa( ac, 1.234567, 8 );
    rvos_printf( "float converted by floattoa: %s\n", ac );
    rvos_floattoa( ac, 34.567, 8 );
    rvos_printf( "float converted by floattoa: %s\n", ac );
#endif    

    printf( "float from printf: %f\n", 45.678 );

    float f1 = 1.0;
    float f2 = 20.2;
    float fm1 = -1.342;
    float fr = f2 * fm1;
    float fd = 1000.0 / 3.0;
    float fs = sqrt( fd );

    printf( "division result: %f, square root %f\n", fd, fs );

#if 0
    rvos_print_text( "calling floattoa\n" );
    rvos_floattoa( ac, fr, 6 );
    rvos_printf( "float converted with rvos_floattoa: %s\n", ac );
#endif    

    printf( "result of 20.2 * -1.342: %f\n", fr );

    double d = (double) fr;
    printf( "now printing fr as a double:\n" );
    printf( "result of 20.2 * -1.342 as a double: %lf\n", d );

    printf( "stop\n" );
    exit( 0 );
} //main


