#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

bool is_parity_even8( uint8_t x ) // unused by apps and expensive to compute.
{
#ifdef _M_AMD64
    return ( ! ( __popcnt16( x ) & 1 ) ); // less portable, but faster. Not on Q9650 CPU 
#elif defined( __APPLE__ )
    return ( ! ( std::bitset<8>( x ).count() & 1 ) );
#else
    printf( "result3\n" );
    return ( ( ~ ( x ^= ( x ^= ( x ^= x >> 4 ) >> 2 ) >> 1 ) ) & 1 );
#endif
} //is_parity_even8

int main( int argc, char * argv[] )
{
    uint16_t ui16 = 0x963b;
    uint8_t x = (uint8_t) ui16;
    bool result = is_parity_even8( x );
    printf( "result for %#x: %d\n", (int) x, (int) result );
}