#include <stdio.h>
#include <stdint.h>
#include <cmath>
#include <float.h>

typedef __int128 int128_t;
typedef unsigned __int128 uint128_t;

union int128_t_union
{
    int128_t v;
    uint64_t parts[ 2 ];
    void init( uint64_t hi, uint64_t lo ) { parts[ 0 ] = lo; parts[ 1 ] = hi; }
};

union uint128_t_union
{
    uint128_t v;
    uint64_t parts[ 2 ];
    void init( uint64_t hi, uint64_t lo ) { parts[ 0 ] = lo; parts[ 1 ] = hi; }
};

//#pragma GCC optimize("O0")

template <class T> void cmp( T a, T b )
{
    bool gt = ( a > b );
    bool lt = ( a < b );
    bool eq = ( a == b );
    bool le = ( a <= b );
    bool ge = ( a >= b );
    printf( "  lt %d le %d eq %d ge %d gt %d\n", lt, le, eq, ge, gt );
} //cmp

void cmp_double( double a, double b )
{
    double diff = a - b;
    double abs_diff = fabs( diff );
    bool gt = ( diff > 0.0 && abs_diff > DBL_EPSILON );
    bool lt = ( diff < 0.0 && abs_diff > DBL_EPSILON );
    bool eq = ( abs_diff < DBL_EPSILON );
    bool le = ( diff <= 0.0 || abs_diff < DBL_EPSILON );
    bool ge = ( diff >= 0.0 || abs_diff < DBL_EPSILON );
    printf( "  lt %d le %d eq %d ge %d gt %d\n", lt, le, eq, ge, gt );
} //cmp_double

void cmp_long_double( long double a, long double b )
{
    long double diff = a - b;
    long double abs_diff = fabsl( diff );
    bool gt = ( diff > 0.0 && abs_diff > LDBL_EPSILON );
    bool lt = ( diff < 0.0 && abs_diff > LDBL_EPSILON );
    bool eq = ( abs_diff < DBL_EPSILON );
    bool le = ( diff <= 0.0 || abs_diff < LDBL_EPSILON );
    bool ge = ( diff >= 0.0 || abs_diff < LDBL_EPSILON );
    printf( "  lt %d le %d eq %d ge %d gt %d\n", lt, le, eq, ge, gt );
} //cmp_long_double

void cmp_float( float a, float b )
{
    float diff = a - b;
    float abs_diff = fabsf( diff );
    bool gt = ( diff > 0.0 && abs_diff > FLT_EPSILON );
    bool lt = ( diff < 0.0 && abs_diff > FLT_EPSILON );
    bool eq = ( abs_diff < FLT_EPSILON );
    bool le = ( diff <= 0.0 || abs_diff < FLT_EPSILON );
    bool ge = ( diff >= 0.0 || abs_diff < FLT_EPSILON );
    printf( "  lt %d le %d eq %d ge %d gt %d\n", lt, le, eq, ge, gt );
} //cmp_float

int main( int argc, char * argv[] )
{
    printf( "uint8_t:\n" );
    cmp( (uint8_t) 1, (uint8_t) 3 );
    cmp( (uint8_t) 1, (uint8_t) -3 );
    cmp( (uint8_t) -1, (uint8_t) 3 );
    cmp( (uint8_t) -1, (uint8_t) -3 );
    cmp( (uint8_t) -1, (uint8_t) -1 );
    cmp( (uint8_t) 1, (uint8_t) -1 );
    cmp( (uint8_t) 247, (uint8_t) 3 );
    cmp( (uint8_t) 247, (uint8_t) -3 );
    cmp( (uint8_t) -247, (uint8_t) 3 );
    cmp( (uint8_t) -247, (uint8_t) -3 );
    cmp( (uint8_t) 0xf1, (uint8_t) 0xf2 );
    cmp( (uint8_t) 0xf3, (uint8_t) 0xf2 );
    cmp( (uint8_t) 0xe1, (uint8_t) 0xe2 );
    cmp( (uint8_t) 0xe3, (uint8_t) 0xe2 );
    cmp( (uint8_t) 0x81, (uint8_t) 0x78 );
    cmp( (uint8_t) 0, (uint8_t) 0x80 );
    cmp( (uint8_t) 0x7f, (uint8_t) 0x80 );

    printf( "int8_t:\n" );
    cmp( (int8_t) 1, (int8_t) 3 );
    cmp( (int8_t) 1, (int8_t) -3 );
    cmp( (int8_t) -1, (int8_t) 3 );
    cmp( (int8_t) -1, (int8_t) -3 );
    cmp( (int8_t) -1, (int8_t) -1 );
    cmp( (int8_t) 1, (int8_t) -1 );
    cmp( (int8_t) 247, (int8_t) 3 );
    cmp( (int8_t) 247, (int8_t) -3 );
    cmp( (int8_t) -247, (int8_t) 3 );
    cmp( (int8_t) -247, (int8_t) -3 );
    cmp( (int8_t) 0xf1, (int8_t) 0xf2 );
    cmp( (int8_t) 0xf3, (int8_t) 0xf2 );
    cmp( (int8_t) 0xe1, (int8_t) 0xe2 );
    cmp( (int8_t) 0xe3, (int8_t) 0xe2 );
    cmp( (int8_t) 0x81, (int8_t) 0x78 );
    cmp( (int8_t) 0, (int8_t) 0x80 );
    cmp( (int8_t) 0x7f, (int8_t) 0x80 );

    printf( "uint16_t:\n" );
    cmp( (uint16_t) 1, (uint16_t) 3 );
    cmp( (uint16_t) 1, (uint16_t) -3 );
    cmp( (uint16_t) -1, (uint16_t) 3 );
    cmp( (uint16_t) -1, (uint16_t) -3 );
    cmp( (uint16_t) -1, (uint16_t) -1 );
    cmp( (uint16_t) 1, (uint16_t) -1 );
    cmp( (uint16_t) 247, (uint16_t) 3 );
    cmp( (uint16_t) 247, (uint16_t) -3 );
    cmp( (uint16_t) -247, (uint16_t) 3 );
    cmp( (uint16_t) -247, (uint16_t) -3 );
    cmp( (uint16_t) 0xff11, (uint16_t) 0xff22 );
    cmp( (uint16_t) 0xff33, (uint16_t) 0xff22 );
    cmp( (uint16_t) 0xef11, (uint16_t) 0xef22 );
    cmp( (uint16_t) 0xef33, (uint16_t) 0xef22 );
    cmp( (uint16_t) 0x8001, (uint16_t) 0x7ff8 );
    cmp( (uint16_t) 0, (uint16_t) 0x8000 );
    cmp( (uint16_t) 0x7fff, (uint16_t) 0x8000 );

    printf( "int16_t:\n" );
    cmp( (int16_t) 1, (int16_t) 3 );
    cmp( (int16_t) 1, (int16_t) -3 );
    cmp( (int16_t) -1, (int16_t) 3 );
    cmp( (int16_t) -1, (int16_t) -3 );
    cmp( (int16_t) -1, (int16_t) -1 );
    cmp( (int16_t) 1, (int16_t) -1 );
    cmp( (int16_t) 247, (int16_t) 3 );
    cmp( (int16_t) 247, (int16_t) -3 );
    cmp( (int16_t) -247, (int16_t) 3 );
    cmp( (int16_t) -247, (int16_t) -3 );
    cmp( (int16_t) 0xff11, (int16_t) 0xff22 );
    cmp( (int16_t) 0xff33, (int16_t) 0xff22 );
    cmp( (int16_t) 0xef11, (int16_t) 0xef22 );
    cmp( (int16_t) 0xef33, (int16_t) 0xef22 );
    cmp( (int16_t) 0x8001, (int16_t) 0x7ff8 );
    cmp( (int16_t) 0, (int16_t) 0x8000 );
    cmp( (int16_t) 0x7fff, (int16_t) 0x8000 );

    printf( "uint32_t:\n" );
    cmp( (uint32_t) 1, (uint32_t) 3 );
    cmp( (uint32_t) 1, (uint32_t) -3 );
    cmp( (uint32_t) -1, (uint32_t) 3 );
    cmp( (uint32_t) -1, (uint32_t) -3 );
    cmp( (uint32_t) -1, (uint32_t) -1 );
    cmp( (uint32_t) 1, (uint32_t) -1 );
    cmp( (uint32_t) 247, (uint32_t) 3 );
    cmp( (uint32_t) 247, (uint32_t) -3 );
    cmp( (uint32_t) -247, (uint32_t) 3 );
    cmp( (uint32_t) -247, (uint32_t) -3 );
    cmp( (uint32_t) 0xffff1111, (uint32_t) 0xffff2222 );
    cmp( (uint32_t) 0xffff3333, (uint32_t) 0xffff2222 );
    cmp( (uint32_t) 0xefff1111, (uint32_t) 0xefff2222 );
    cmp( (uint32_t) 0xefff3333, (uint32_t) 0xefff2222 );
    cmp( (uint32_t) 0x80000001, (uint32_t) 0x7ffffff8 );
    cmp( (uint32_t) 0, (uint32_t) 0x80000000 );
    cmp( (uint32_t) 0x7fffffff, (uint32_t) 0x80000000 );

    printf( "int32_t:\n" );
    cmp( (int32_t) 1, (int32_t) 3 );
    cmp( (int32_t) 1, (int32_t) -3 );
    cmp( (int32_t) -1, (int32_t) 3 );
    cmp( (int32_t) -1, (int32_t) -3 );
    cmp( (int32_t) -1, (int32_t) -1 );
    cmp( (int32_t) 1, (int32_t) -1 );
    cmp( (int32_t) 247, (int32_t) 3 );
    cmp( (int32_t) 247, (int32_t) -3 );
    cmp( (int32_t) -247, (int32_t) 3 );
    cmp( (int32_t) -247, (int32_t) -3 );
    cmp( (int32_t) 0xffff1111, (int32_t) 0xffff2222 );
    cmp( (int32_t) 0xffff3333, (int32_t) 0xffff2222 );
    cmp( (int32_t) 0xefff1111, (int32_t) 0xefff2222 );
    cmp( (int32_t) 0xefff3333, (int32_t) 0xefff2222 );
    cmp( (int32_t) 0x80000001, (int32_t) 0x7ffffff8 );
    cmp( (int32_t) 0, (int32_t) 0x80000000 );
    cmp( (int32_t) 0x7fffffff, (int32_t) 0x80000000 );

    printf( "uint64_t:\n" );
    cmp( (uint64_t) 1, (uint64_t) 3 );
    cmp( (uint64_t) 1, (uint64_t) -3 );
    cmp( (uint64_t) -1, (uint64_t) 3 );
    cmp( (uint64_t) -1, (uint64_t) -3 );
    cmp( (uint64_t) -1, (uint64_t) -1 );
    cmp( (uint64_t) 1, (uint64_t) -1 );
    cmp( (uint64_t) 247, (uint64_t) 3 );
    cmp( (uint64_t) 247, (uint64_t) -3 );
    cmp( (uint64_t) -247, (uint64_t) 3 );
    cmp( (uint64_t) -247, (uint64_t) -3 );
    cmp( (uint64_t) 0xffff111111111111, (uint64_t) 0xffff222222222222 );
    cmp( (uint64_t) 0xffff333333333333, (uint64_t) 0xffff222222222222 );
    cmp( (uint64_t) 0xefff111111111111, (uint64_t) 0xefff222222222222 );
    cmp( (uint64_t) 0xefff333333333333, (uint64_t) 0xefff222222222222 );
    cmp( (uint64_t) 0x8000000000000001, (uint64_t) 0x7ffffffffffffff8 );
    cmp( (uint64_t) 0, (uint64_t) 0x8000000000000000 );
    cmp( (uint64_t) 0x7fffffffffffffff, (uint64_t) 0x8000000000000000 );

    printf( "int64_t:\n" );
    cmp( (int64_t) 1, (int64_t) 3 );
    cmp( (int64_t) 1, (int64_t) -3 );
    cmp( (int64_t) -1, (int64_t) 3 );
    cmp( (int64_t) -1, (int64_t) -3 );
    cmp( (int64_t) -1, (int64_t) -1 );
    cmp( (int64_t) 1, (int64_t) -1 );
    cmp( (int64_t) 247, (int64_t) 3 );
    cmp( (int64_t) 247, (int64_t) -3 );
    cmp( (int64_t) -247, (int64_t) 3 );
    cmp( (int64_t) -247, (int64_t) -3 );
    cmp( (int64_t) 0xffff111111111111, (int64_t) 0xffff222222222222 );
    cmp( (int64_t) 0xffff333333333333, (int64_t) 0xffff222222222222 );
    cmp( (int64_t) 0xefff111111111111, (int64_t) 0xefff222222222222 );
    cmp( (int64_t) 0xefff333333333333, (int64_t) 0xefff222222222222 );
    cmp( (int64_t) 0x8000000000000001, (int64_t) 0x7ffffffffffffff8 );
    cmp( (int64_t) 0, (int64_t) 0x8000000000000000 );
    cmp( (int64_t) 0x7fffffffffffffff, (int64_t) 0x8000000000000000 );

    printf( "uint128_t:\n" );
    uint128_t_union ui128a, ui128b;

    ui128a.init( 0, 1 );
    ui128b.init( 0, 3 );
    cmp( ui128a.v, ui128b.v );

    ui128a.init( 0, 1 );
    ui128b.init( ~0ull, -3 );
    cmp( ui128a.v, ui128b.v );

    ui128a.init( ~0ull, -1 );
    ui128b.init( 0, 3 );
    cmp( ui128a.v, ui128b.v );

    ui128a.init( ~0ull, -1 );
    ui128b.init( ~0ull, -3 );
    cmp( ui128a.v, ui128b.v );

    ui128a.init( ~0ull, -1 );
    ui128b.init( ~0ull, -1 );
    cmp( ui128a.v, ui128b.v );

    ui128a.init( 0, 1 );
    ui128b.init( ~0ull, -1 );
    cmp( ui128a.v, ui128b.v );

    ui128a.init( 0, 247 );
    ui128b.init( 0, 3 );
    cmp( ui128a.v, ui128b.v );

    ui128a.init( 0, 247 );
    ui128b.init( ~0ull, -3 );
    cmp( ui128a.v, ui128b.v );

    ui128a.init( ~0ull, -247 );
    ui128b.init( 0, 3 );
    cmp( ui128a.v, ui128b.v );

    ui128a.init( ~0ull, -247 );
    ui128b.init( ~0ull, -3 );
    cmp( ui128a.v, ui128b.v );

    ui128a.init( 0xffff111111111111, 0x1111111111111111 );
    ui128b.init( 0xffff222222222222, 0x2222222222222222 );
    cmp( ui128a.v, ui128b.v );

    ui128a.init( 0xffff333333333333, 0x3333333333333333 );
    ui128b.init( 0xffff222222222222, 0x2222222222222222 );
    cmp( ui128a.v, ui128b.v );

    ui128a.init( 0xefff111111111111, 0x1111111111111111 );
    ui128b.init( 0xefff222222222222, 0x2222222222222222 );
    cmp( ui128a.v, ui128b.v );

    ui128a.init( 0xefff333333333333, 0x3333333333333333 );
    ui128b.init( 0xefff222222222222, 0x2222222222222222 );
    cmp( ui128a.v, ui128b.v );

    ui128a.init( 0x8000000000000000, 0x0000000000000001 );
    ui128b.init( 0x7fffffffffffffff, 0xfffffffffffffff8 );
    cmp( ui128a.v, ui128b.v );

    ui128a.init( 0, 0 );
    ui128b.init( 0x8000000000000000, 0 );
    cmp( ui128a.v, ui128b.v );

    ui128a.init( 0x7fffffffffffffff, 0xffffffffffffffff );
    ui128b.init( 0x8000000000000000, 0 );
    cmp( ui128a.v, ui128b.v );    

    printf( "int128_t:\n" );

    int128_t_union i128a, i128b;

    i128a.init( 0, 1 );
    i128b.init( 0, 3 );
    cmp( i128a.v, i128b.v );

    i128a.init( 0, 1 );
    i128b.init( ~0ull, -3 );
    cmp( i128a.v, i128b.v );

    i128a.init( ~0ull, -1 );
    i128b.init( 0, 3 );
    cmp( i128a.v, i128b.v );

    i128a.init( ~0ull, -1 );
    i128b.init( ~0ull, -3 );
    cmp( i128a.v, i128b.v );

    i128a.init( ~0ull, -1 );
    i128b.init( ~0ull, -1 );
    cmp( i128a.v, i128b.v );

    i128a.init( 0, 1 );
    i128b.init( ~0ull, -1 );
    cmp( i128a.v, i128b.v );

    i128a.init( 0, 247 );
    i128b.init( 0, 3 );
    cmp( i128a.v, i128b.v );

    i128a.init( 0, 247 );
    i128b.init( ~0ull, -3 );
    cmp( i128a.v, i128b.v );

    i128a.init( ~0ull, -247 );
    i128b.init( 0, 3 );
    cmp( i128a.v, i128b.v );

    i128a.init( ~0ull, -247 );
    i128b.init( ~0ull, -3 );
    cmp( i128a.v, i128b.v );

    i128a.init( 0xffff111111111111, 0x1111111111111111 );
    i128b.init( 0xffff222222222222, 0x2222222222222222 );
    cmp( i128a.v, i128b.v );

    i128a.init( 0xffff333333333333, 0x3333333333333333 );
    i128b.init( 0xffff222222222222, 0x2222222222222222 );
    cmp( i128a.v, i128b.v );

    i128a.init( 0xefff111111111111, 0x1111111111111111 );
    i128b.init( 0xefff222222222222, 0x2222222222222222 );
    cmp( i128a.v, i128b.v );

    i128a.init( 0xefff333333333333, 0x3333333333333333 );
    i128b.init( 0xefff222222222222, 0x2222222222222222 );
    cmp( i128a.v, i128b.v );

    i128a.init( 0x8000000000000000, 0x0000000000000001 );
    i128b.init( 0x7fffffffffffffff, 0xfffffffffffffff8 );
    cmp( i128a.v, i128b.v );

    i128a.init( 0, 0 );
    i128b.init( 0x8000000000000000, 0 );
    cmp( i128a.v, i128b.v );

    i128a.init( 0x7fffffffffffffff, 0xffffffffffffffff );
    i128b.init( 0x8000000000000000, 0 );
    cmp( i128a.v, i128b.v );

    printf( "floating point:\n" );
    float f = -0.5f;
    double d = -0.5;
    long double ld = -0.5L;
    for ( int i = 0; i < 10; i++ )
    {
        cmp_float( f, 0.2f );
        cmp_double( d, 0.2 );
        cmp_long_double( ld, 0.2L );
        f += 0.1f;
        d += 0.1;
        ld += 0.1L;
    }
} //main
