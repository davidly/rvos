#include <stdint.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <unistd.h>


#pragma clang diagnostic ignored "-Wnan-infinity-disabled"

const char * tf( bool f )
{
    return f ? "true" : "false";
}

void show_num( double d )
{
    printf( "    value: %lf = %#llx, isnan %s, isinf %s, iszero %s, signbit %s\n", * (uint64_t *) &d,
            (double) d, tf( isnan( d ) ), tf( isinf( d ) ),
            tf( 0.0 == d ),  tf( signbit( d ) ) );
} //show_num

template <class T> T do_math( T a, T b )
{
    printf( "  in do_math()\n" );
    show_num( a );
    show_num( b );

    T r = a * b;
    show_num( r );

    r = a / b;
    show_num( r );

    r = a + b;
    show_num( r );

    r = a - b;
    show_num( r );

    r = a * a * a * a * b * b * b * b;
    show_num( r );

    return r;
}

int main( int argc, char * argv[] )
{
    double d;

    d = NAN;
    printf( "NAN: %#llx\n", * (uint64_t *) &d );
    d = -NAN;
    printf( "-NAN: %#llx\n", * (uint64_t *) &d );
    d = INFINITY;
    printf( "INFINITY: %#llx\n", * (uint64_t *) &d );
    d = -INFINITY;
    printf( "-INFINITY: %#llx\n", * (uint64_t *) &d );

    printf( "testing with invalid double:\n" );
    uint64_t x = 0x7ff8000000000000;
    memcpy( &d, &x, 8 );
    do_math( d, 0.0 );
    do_math( 3.0, d );
    do_math( d, d );

    printf( "testing with NAN:\n" );
    d = NAN;
    do_math( d, 0.0 );
    do_math( 3.0, d );
    do_math( d, d );

    printf( "testing with INFINITY:\n" );
    d = INFINITY;
    do_math( d, 0.0 );
    do_math( 3.0, d );
    do_math( d, d );

    printf( "testing with -INFINITY:\n" );
    d = -INFINITY;
    do_math( d, 0.0 );
    do_math( 3.0, d );
    do_math( d, d );

    printf( "testing with 69:\n" );
    d = 69.0;
    do_math( d, 0.0 );
    do_math( 3.0, d );
    do_math( d, d );

    printf( "testing with div by 0:\n" );
    d = 0.0 / 0.0;
    do_math( d, 0.0 );
    do_math( 3.0, d );
    do_math( d, d );

    printf( "nan test completed with great success\n" );
    return 0;
}