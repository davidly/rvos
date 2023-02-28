#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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
 
char * ui64toa( uint64_t num, char * str, int base )
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
} //ui64toa

char * ui32toa( uint32_t num, char * str, int base )
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
} //ui32toa

template <class T> T myabs( T x )
{
    if ( x < 0 )
        return -x;
    return x;
} //myabs

extern "C" void riscv_print_text( const char * p );

#if defined( _MSC_VER ) || defined(WIN64)

extern "C" void riscv_print_text( const char * p )
{
    printf( "%s", p );
}
#endif

template <class T> void show_result( T x )
{
    static char buf[ 128 ];

    if ( sizeof( T ) == 4 )
        ui32toa( x, buf, 16 );
    else
        ui64toa( x, buf, 16 );
    riscv_print_text( "result: " );
    riscv_print_text( buf );
    riscv_print_text( "\n" );
} //show_result

#pragma GCC optimize ("O0")
extern "C" int main()
{
    int8_t i8 = -1;
    i8 >>= 1;
    show_result( (uint8_t) i8 );

    uint8_t ui8 = 0xff;
    ui8 >>= 1;
    show_result( ui8 );

    int16_t i16 = -1;
    i16 >>= 1;
    show_result( (uint16_t) i16 );

    uint16_t ui16 = 0xffff;
    ui16 >>= 1;
    show_result( ui16 );

    int32_t i32 = -1;
    i32 >>= 1;
    show_result( (uint32_t) i32 );

    uint32_t ui32 = 0xffffffff;
    ui32 >>= 1;
    show_result( ui32 );

    int64_t i64 = -1;
    i64 >>= 1;
    show_result( i64 );

    uint64_t ui64 = 0xffffffffffffffff;
    ui64 >>= 1;
    show_result( ui64 );

    riscv_print_text( "now test left shifts\n" );

    i8 = -1;
    i8 <<= 1;
    show_result( (uint8_t) i8 );

    ui8 = 0xff;
    ui8 <<= 1;
    show_result( ui8 );

    i16 = -1;
    i16 <<= 1;
    show_result( (uint16_t) i16 );

    ui16 = 0xffff;
    ui16 <<= 1;
    show_result( ui16 );

    i32 = -1;
    i32 <<= 1;
    show_result( (uint32_t) i32 );

    ui32 = 0xffffffff;
    ui32 <<= 1;
    show_result( ui32 );

    i64 = -1;
    i64 <<= 1;
    show_result( i64 );

    ui64 = 0xffffffffffffffff;
    ui64 <<= 1;
    show_result( ui64 );

    riscv_print_text( "stop\n" );
    return 0;
} //main


