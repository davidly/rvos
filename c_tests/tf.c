#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define _USE_MATH_DEFINES 
#include <math.h>
#include <float.h>
#include <stdint.h>

char *floattoa( char *buffer, double d, int precision )
{
    char * pbuf = buffer;

    if ( d < 0 )
    {
        *pbuf++ = '-';
        *pbuf = 0;
        d *= -1;
    }

    uint32_t whole = (uint32_t) (float) d;
    sprintf( pbuf, "%u", whole );

    if ( precision > 0 )
    {
        pbuf = buffer + strlen( buffer );
        *pbuf++ = '.';

        double fraction = d - whole;

        while ( precision > 0 )
        {
            fraction *= 10.0;
            whole = fraction;
            *pbuf++ = '0' + whole;

            fraction -= whole;
            precision--;
        }

        *pbuf = 0;
    }

    return buffer;
} //floattoa

#define TRIG_FLT_EPSILON 0.00002  // 0.00000011920928955078
#define TRIG_DBL_EPSILON 0.00000002 // 0.00000000000000022204
#define TRIG_LDBL_EPSILON 0.0000000000000002 // 0.0000000000000000000000000000000001925930

void check_same_f( const char * operation, float a, float b )
{
    float diff = a - b;
    float abs_diff = fabsf( diff );
    bool eq = ( abs_diff <= ( TRIG_FLT_EPSILON ) );
    if ( !eq )
        printf( "operation %s: float %.20f is not the same as float %.20f\n", operation, a, b );
} //check_same_f

void check_same_d( const char * operation, double a, double b )
{
    double diff = a - b;
    double abs_diff = fabs( diff );
    bool eq = ( abs_diff <= ( TRIG_DBL_EPSILON ) );
    if ( !eq )
        printf( "operation %s: double %.20lf is not the same as double %.20lf\n", operation, a, b );
} //check_same_d

void check_same_ld( const char * operation, long double a, long double b )
{
    long double diff = a - b;
    long double abs_diff = fabsl( diff );
    bool eq = ( abs_diff <= ( TRIG_LDBL_EPSILON ) );
    if ( !eq )
    {
        printf( "operation %s: long double %.20Lf is not the same as long double %.20Lf\n", operation, a, b );
        exit( 0 );
    }
} //check_same_ld

void many_trigonometrics()
{
    float f = ( -M_PI / 2 ) + 0x000001; // want to be >= negative half pi.

    //printf( "float epsilon: %.40lf\n", (double) FLT_EPSILON );
    //printf( "double epsilon: %.40lf\n", (double) DBL_EPSILON );
    //printf( "long double epsilon: %.40lf\n", (double) LDBL_EPSILON );

    while ( f < ( M_PI / 2 ) )
    {
        float fresult = tanf( f );
        float fback = atanf( fresult );
        check_same_f( "tan", f, fback );

        double dresult = tan( (double) f );
        double dback = atan( dresult );
        check_same_d( "tan", (double) f, dback );

        long double ldresult = tanl( (long double) f );
        long double ldback = atanl( ldresult );
        check_same_ld( "tan", (long double) f, ldback );

        fresult = sinf( f );
        fback = asinf( fresult );
        check_same_f( "sin", f, fback );

        dresult = sin( (double) f );
        dback = asin( dresult );
        check_same_d( "sin", (double) f, dback );

        ldresult = sinl( (long double) f );
        ldback = asinl( ldresult );
        check_same_ld( "sin", (long double) f, ldback );

        float f_cos = f + ( M_PI / 2 );
        fresult = cosf( f_cos );
        fback = acosf( fresult );
        check_same_f( "cos", f_cos, fback );

        dresult = cos( (double) f_cos );
        dback = acos( dresult );
        check_same_d( "cos", (double) f_cos, dback );

        ldresult = cosl( f_cos );
        ldback = acosl( ldresult );
        check_same_ld( "cos", (long double) f_cos, ldback );

        f += .01f;
    }
} //many_trignometrics

#pragma GCC optimize ("O0")

extern "C" int main()
{
    char ac[ 100 ];
    
    floattoa( ac, -1.234567, 8 );
    printf( "float converted by floattoa: %s\n", ac );
    floattoa( ac, 1.234567, 8 );
    printf( "float converted by floattoa: %s\n", ac );
    floattoa( ac, 34.567, 8 );
    printf( "float converted by floattoa: %s\n", ac );

    printf( "float from printf: %f\n", 45.678 );

    float f1 = 1.0;
    float f2 = 20.2;
    float fm1 = -1.342;
    float fr = f2 * fm1;
    float fd = 1000.0 / 3.0;
    float fs = sqrtf( fd );

    printf( "division result: %f, square root %f\n", fd, fs );

    floattoa( ac, fr, 6 );
    printf( "float converted with floattoa: %s\n", ac );

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

    many_trigonometrics();

    printf( "test tf completed with great success\n" );
    exit( 0 );
} //main


