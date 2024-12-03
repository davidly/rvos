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
    float fs = sqrtf( fd );

    printf( "division result: %f, square root %f\n", fd, fs );

#if 0
    rvos_print_text( "calling floattoa\n" );
    rvos_floattoa( ac, fr, 6 );
    rvos_printf( "float converted with rvos_floattoa: %s\n", ac );
#endif    

    printf( "result of 20.2 * -1.342: %f\n", fr );

    double d = (double) fr;
    printf( "result of 20.2 * -1.342 as a double: %lf\n", d );

    float pi = 3.14159265358979323846264338327952884197169399375105820974944592307;
    float radians = pi / 180.0 * 30.0;
    printf( "pi in radians: %f\n", radians );

    float s = sinf( radians );
    printf( "sinf of 30 degress is %lf\n", s );

    float c = cosf( radians );
    printf( "cosf of 30 degrees is %lf\n", c );

    float t = tanf( radians );
    printf( "tanf of 30 degrees is %lf\n", t );

    float f = atof( "1.0" );
    float at = atanf( f );
    printf( "atanf of %lf is %lf\n", f, at );

    at = atan2f( 0.3, 0.2 );
    printf( "atan2f of 0.3, 0.2 is %lf\n", at );

    c = acosf( 0.3 );
    printf( "acosf of 0.3 is %lf\n", c );

    s = asinf( 0.3 );
    printf( "asinf of 0.3 is %lf\n", s );

    f = tanhf( 2.2 );
    printf( "tanhf of 2.2 is %lf\n", s );
    
    f = logf( 0.3 );
    printf( "logf of 0.3: %lf\n", f );

    f = log10f( 300.0 );
    printf( "log10f of 300: %lf\n", f );
    
    int exponent;
    float mantissa = frexpf( pi, &exponent );
    printf( "pi has mantissa: %lf, exponent %d\n", mantissa, exponent );

    float b = 2.7;
    for ( float a = 2.0; a < 3.0; a += 0.1 )
    {
        if ( a > b )
            printf( "g," );
        if ( a >= b )
            printf( "ge," );
        if ( a == b )
            printf( "eq," );
        if ( a < b )
            printf( "l," );
        if ( a <= b )
            printf( "le," );
    }
    printf( "\n" );

    printf( "stop\n" );
    exit( 0 );
} //main


