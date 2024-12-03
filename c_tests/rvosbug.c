#include <stdio.h>
#include <stdint.h>

#pragma GCC optimize ("O2")
//#pragma GCC optimize ("no-inline")

static int64_t sign_extend( uint64_t x, uint64_t bits )
{
    const int64_t m = ( (uint64_t) 1 ) << bits;
    return ( x ^ m ) - m;
} //sign_extend

void runtest( uint32_t t, uint32_t shift )
{
    uint64_t result = sign_extend( t >> shift, 31 - shift );
    printf( "result (0xfffffffffffffff0 expected): %#llx\n", result );
}

int main( int argc, char * argv[] )
{
    uint64_t rv = 0xfffffffff0000000ull;
    runtest( rv & 0xffffffff, 24 );
    return 0;
}
