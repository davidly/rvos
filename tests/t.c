#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void swap( char & a, char & b )
{
    char c = a;
    a = b;
    b = c;
} //swap

void reverse( char str[], int length )
{
    int start = 0;
    int end = length - 1;
    while ( start < end )
    {
        swap( * ( str + start ), * ( str + end ) );
        start++;
        end--;
    }
} //reverse
 
char * i64toa( int64_t num, char * str, int base )
{
    int i = 0;
    bool isNegative = false;
 
    if ( 0 == num )
    {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }
 
    if ( num < 0 && 10 == base )
    {
        isNegative = true;
        num = -num;
    }
 
    while ( 0 != num )
    {
        int rem = num % base;
        str[i++] = (rem > 9)? (rem-10) + 'a' : rem + '0';
        num = num/base;
    }
 
    if (isNegative)
        str[i++] = '-';
 
    str[i] = '\0';
 
    reverse( str, i );
 
    return str;
} //i64toa

template <class T> T myabs( T x )
{
    if ( x < 0 )
        return -x;
    return x;
} //myabs

//#pragma GCC optimize ("O0")
template <class T> T test( T & min, T & max )
{
    T a[ 340 ];

    for ( uint64_t i = 0; i < sizeof( a ) / sizeof( a[0] ); i++ )
        a[ i ] = i;

    for ( size_t zz = 0; zz < 10; zz++ )
    {
        for ( T i = min; i < max; i++ )
        {
            T j = 13 - i;
            int x = (int) j;
            int y = x * 2;
            a[ myabs( i ) ] = (T) y;
            a[ myabs( i + 1 ) ] = a[ myabs( i ) + 2 ] | a[ myabs( i ) + 3 ];
            a[ myabs( i + 2 ) ] = a[ myabs( i ) + 3 ] & a[ myabs( i ) + 4 ];
            a[ myabs( i + 3 ) ] = a[ myabs( i ) + 4 ] + a[ myabs( i ) + 5 ];
            a[ myabs( i + 4 ) ] = a[ myabs( i ) + 5 ] - a[ myabs( i ) + 6 ];
            a[ myabs( i + 5 ) ] = a[ myabs( i ) + 6 ] * a[ myabs( i ) + 7 ];
            if ( 0 != a[ myabs( i ) + 8 ] )
                a[ myabs( i + 6 ) ] = a[ myabs( i ) + 7 ] / a[ myabs( i ) + 8 ];
            a[ myabs( i + 7 ) ] = a[ myabs( i ) + 8 ] ^ a[ myabs( i ) + 9 ];
            if ( 0 != a[ myabs( i ) + 10 ] )
                a[ myabs( i + 8 ) ] = a[ myabs( i ) + 9 ] % a[ myabs( i ) + 10 ];
            a[ myabs( i + 9 ) ] = a[ myabs( i ) + 8 ] << a[ myabs( i ) + 11 ];
            a[ myabs( i + 10 ) ] = a[ myabs( i ) + 8 ] >> a[ myabs( i ) + 12 ];
            a[ myabs( i + 11 ) ] = ( a[ myabs( i ) + 8 ] << 3 );
            a[ myabs( i + 12 ) ] = ( a[ myabs( i ) + 8 ] >> 4 );

            a[ myabs( i + 12 ) ] &= 0x10;
            a[ myabs( i + 13 ) ] |= 0x10;
            a[ myabs( i + 14 ) ] ^= 0x10;
            a[ myabs( i + 12 ) ] += 7;
            a[ myabs( i + 13 ) ] -= 6;
            a[ myabs( i + 14 ) ] *= 5;
            a[ myabs( i + 14 ) ] /= 4;
        }
    }

    return a[ 10 ];
} //template

extern "C" void riscv_print_text( const char * p );

template <class T> void show_result( T x )
{
    static char buf[ 128 ];

    i64toa( x, buf, 10 );
    riscv_print_text( "result: " );
    riscv_print_text( buf );
    riscv_print_text( "\n" );
} //show_result

extern "C" int main()
{
    riscv_print_text( "int8_t\n" );
    int8_t i8min = -128, i8max = 127;
    int8_t i8 = test( i8min, i8max );
    show_result( (int64_t) i8 );

    riscv_print_text( "uint8_t\n" );
    uint8_t ui8min = 0, ui8max = 255;
    uint8_t u8 = test( ui8min, ui8max );
    show_result( (uint64_t) i8 );

    riscv_print_text( "int16_t\n" );
    int16_t i16min = -228, i16max = 227;
    int16_t i16 = test( i16min, i16max );
    show_result( (int64_t) i16 );

    riscv_print_text( "uint16_t\n" );
    uint16_t ui16min = 0, ui16max = 300;
    uint16_t u16 = test( ui16min, ui16max );
    show_result( (uint64_t) u16 );

    riscv_print_text( "int32_t\n" );
    int32_t i32min = -228, i32max = 227;
    int32_t i32 = test( i32min, i32max );
    show_result( (int64_t) i32 );

    riscv_print_text( "uint32_t\n" );
    uint32_t ui32min = 0, ui32max = 300;
    uint32_t u32 = test( ui32min, ui32max );
    show_result( (uint64_t) u32 );

    riscv_print_text( "int64_t\n" );
    int64_t i64min = -228, i64max = 227;
    int64_t i64 = test( i64min, i64max );
    show_result( (int64_t) i64 );

    riscv_print_text( "uint64_t\n" );
    uint64_t ui64min = 0, ui64max = 300;
    uint64_t u64 = test( ui64min, ui64max );
    show_result( (uint64_t) u64 );

    riscv_print_text( "stop\n" );
    return 0;
} //main


