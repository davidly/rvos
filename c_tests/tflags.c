#include <stdio.h>
#include <stdint.h>

bool fN = 0;
bool fZ = 0;
bool fC = 0;
bool fV = 0;

uint64_t add_with_carry64( uint64_t x, uint64_t y, bool carry, bool setflags )
{
    uint64_t result = x + y + (uint64_t) carry;

    if ( setflags )
    {
        fN = ( (int64_t) result < 0 );
        fZ = ( 0 == result );

        // no 128-bit integers, so find fC and fV the hard way

        fC = ( ( result < x ) || ( result < y ) || ( result < (uint64_t) carry ) );

        int64_t ix = (int64_t) x;
        int64_t iy = (int64_t) y;
        int64_t iresult = (int64_t) result;
        fV = ( ( ( ix > 0 && iy > 0 ) && ( iresult < ix || iresult < iy ) ) ||
               ( ( ix < 0 && iy < 0 ) && ( iresult > ix || iresult > iy ) ) );
    }
    return result;
} //add_with_carry64

uint64_t sub64( uint64_t x, uint64_t y, bool setflags )
{
    return add_with_carry64( x, ~y, true, setflags );
} //sub64

uint32_t add_with_carry32( uint32_t x, uint32_t y, bool carry, bool setflags )
{
    uint64_t unsigned_sum = (uint64_t) x + (uint64_t) y + (uint64_t) carry;
    uint32_t result = (uint32_t) ( unsigned_sum & 0xffffffff );

    if ( setflags )
    {
        // this method of setting flags is as the Arm documentation suggests
        int64_t signed_sum = (int64_t) (int32_t) x + (int64_t) (int32_t) y + (uint64_t) carry;
        fN = ( (int32_t) result < 0 );
        fZ = ( 0 == result );
        fC = ( (uint64_t) result != unsigned_sum );
        fV = ( (int64_t) (int32_t) result != signed_sum );
        //tracer.Trace( "  addwithcarry32 %#x with %#x, fN %d, fZ %d, fC %d, fV: %d\n", x, y, fN, fZ, fC, fV );
    }
    return result;
} //add_with_carry32

uint32_t sub32( uint32_t x, uint32_t y, bool setflags )
{
    return add_with_carry32( x, ~y, true, setflags );
} //sub32

int main( int argc, char * argv[] )
{
    //uint32_t result32 = sub32( 7, 7, true );
    //printf( "result32: %u, fN %d, fZ %d, fC %d, fV %d\n", result32, fN, fZ, fC, fV );
    uint64_t result64 = sub64( 0, 0, true );
    printf( "result64: %llu, fN %d, fZ %d, fC %d, fV %d\n", result64, fN, fZ, fC, fV );
    return 0;
}
