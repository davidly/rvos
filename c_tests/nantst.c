#include <stdint.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <unistd.h>

#define _perhaps_inline __attribute__((noinline))

// -Ofast doesn't understand infinity so the compiler would emit this warning
// v14 of clang doesn't understand this 
#pragma clang diagnostic ignored "-Wnan-infinity-disabled"

double set_double_sign( double d, bool sign )
{
    uint64_t val = sign ? ( ( * (uint64_t *) &d ) | 0x8000000000000000 ) : ( ( * (uint64_t *) &d ) & 0x7fffffffffffffff );
    return * (double *) &val;
} //set_double_sign

const char * tf( bool f )
{
    return f ? "true" : "false";
}

void _perhaps_inline show_num( double d )
{
    printf( "  %lf = %#llx, isnan %s, isinf %s, iszero %s, signbit %s\n", * (uint64_t *) &d,
            (double) d, tf( isnan( d ) ), tf( isinf( d ) ),
            tf( 0.0 == d ),  tf( signbit( d ) ) );
} //show_num

double _perhaps_inline do_math( double a, double b )
{
    printf( "  in do_math()\n" );
    printf( "       a:" );
    show_num( a );
    printf( "       b:" );
    show_num( b );

    double r = a * b;
    printf( "       *:" );
    show_num( r );

    r = a / b;
    printf( "       /:" );
    show_num( r );

    r = a + b;
    printf( "       +:" );
    show_num( r );

    r = a - b;
    printf( "       -:" );
    show_num( r );

    return r;
}

double zero = 0.0;
double neg_zero = set_double_sign( 0.0, true );
double infinity = INFINITY;
double neg_infinity = set_double_sign( INFINITY, true );
double not_a_number = NAN;
double neg_not_a_number = set_double_sign( NAN, true );

double test_case( double d )
{
    double r = 0.0;
    r += do_math( d, 0.0 );
    r += do_math( 0.0, d );
    r += do_math( d, neg_zero );
    r += do_math( neg_zero, d );
    r += do_math( 3.0, d );
    r += do_math( d, 3.0 );
    r += do_math( -3.0, d );
    r += do_math( d, -3.0 );
    r += do_math( d, not_a_number );
    r += do_math( not_a_number, d );
    r += do_math( d, neg_not_a_number );
    r += do_math( neg_not_a_number, d );
    r += do_math( d, infinity );
    r += do_math( infinity, d );
    r += do_math( d, neg_infinity );
    r += do_math( neg_infinity, d );
    r += do_math( d, d );
    return r;
} //test_case

int main( int argc, char * argv[] )
{
    double d;

    printf( "NAN: %#llx\n", * (uint64_t *) & not_a_number );
    printf( "-NAN: %#llx\n", * (uint64_t *) & neg_not_a_number );
    printf( "INFINITY: %#llx\n", * (uint64_t *) & infinity );
    printf( "-INFINITY: %#llx\n", * (uint64_t *) & neg_infinity );
    printf( "0.0: %#llx\n", * (uint64_t *) & zero );
    printf( "-0.0: %#llx\n", * (uint64_t *) & neg_zero );

    printf( "testing with NAN:\n" );
    test_case( not_a_number );

    printf( "testing with -NAN:\n" );
    test_case( neg_not_a_number );

    printf( "testing with INFINITY:\n" );
    test_case( infinity );

    printf( "testing with -INFINITY:\n" );
    test_case( neg_infinity );

    printf( "testing with 69:\n" );
    test_case( 69.0 );

    printf( "testing with -69:\n" );
    test_case( -69.0 );

    printf( "testing with 0.0:\n" );
    test_case( zero );

    printf( "testing with -0.0:\n" );
    test_case( neg_zero );

    printf( "nan test completed with great success\n" );
    return 0;
}
