#pragma once

// taken from various places on stackoverflow

#include <inttypes.h>

class CMultiply128
{
    private:
        static uint64_t umul_32_32( uint32_t x, uint32_t y ) { return ( (uint64_t) x ) * y; }
    
    public:
        static void mul_u64_u64( uint64_t *rh, uint64_t *rl, uint64_t x, uint64_t y )
        {
            uint32_t xlo = (uint32_t) x;
            uint32_t xhi = (uint32_t) ( x >> 32 );
            uint32_t ylo = (uint32_t) y;
            uint32_t yhi = (uint32_t) ( y >> 32 );
        
            uint64_t m0 = umul_32_32( xlo, ylo );
            uint64_t m1 = umul_32_32( xhi, ylo );
            uint64_t m2 = umul_32_32( xlo, yhi );
            uint64_t m3 = umul_32_32( xhi, yhi );
            m1 += ( m0 >> 32 );
            m3 += ( m2 >> 32 );
            m1 += ( m2 & UINT32_MAX );
        
            *rh = m3 + ( m1 >> 32 );
            *rl = ( m1 << 32 ) | ( m0 & UINT32_MAX );
        } //mul_u64_u64

        static void mul_s64_s64( int64_t *rh, int64_t *rl, int64_t x, int64_t y )
        {
            mul_u64_u64( (uint64_t *) rh, (uint64_t *) rl, (uint64_t) x, (uint64_t) y );

            if ( x < 0 )
                *rh -= y;
            if ( y < 0 )
                *rh -= x;
        } //mul_s64_s64

        static uint64_t mul_u64_u64( uint64_t x, uint64_t y, uint64_t *rh )
        {
            uint64_t rl;
            mul_u64_u64( rh, &rl, x, y );
            return rl;
        } //mul_u64_u64

        static int64_t mul_s64_s64( int64_t x, int64_t y, int64_t *rh )
        {
            int64_t rl;
            mul_s64_s64( rh, &rl, x, y );
            return rl;
        } //mul_s64_s64
};


// Structure to represent a 128-bit unsigned integer
struct UInt128_t {
    uint64_t high;
    uint64_t low;
};

// Function to divide a 128-bit unsigned integer by a 64-bit unsigned integer
inline void divideUInt128ByUInt64( UInt128_t & dividend, uint64_t divisor, uint64_t& quotient, uint64_t& remainder )
{
    if (divisor == 0) {
        // Handle division by zero error as appropriate for your application
        // For this example, we will return 0 for both quotient and remainder.
        quotient = 0;
        remainder = 0;
        return;
    }
    uint64_t q = 0;
    UInt128_t current_remainder = {0, 0};

    // Iterate over all 128 bits of the dividend
    for (int i = 127; i >= 0; --i) {
        // Shift the 128-bit current_remainder left by 1 bit
        current_remainder.high = (current_remainder.high << 1) | (current_remainder.low >> 63);
        current_remainder.low <<= 1;

        // Bring in the next bit of the dividend
        if (i >= 64) {
            current_remainder.low |= (dividend.high >> (i - 64)) & 1;
        } else {
            current_remainder.low |= (dividend.low >> i) & 1;
        }

        // Shift the quotient left by 1
        q <<= 1;

        // Compare the 128-bit current_remainder with the 64-bit divisor
        // If current_remainder >= divisor (promoted to 128-bit)
        if (current_remainder.high > 0 || current_remainder.low >= divisor) {
            // Subtract the divisor from the 128-bit current_remainder
            if (current_remainder.low >= divisor) {
                current_remainder.low -= divisor;
            } else {
                current_remainder.high -= 1;
                current_remainder.low -= divisor; // Subtraction will wrap correctly
            }
            q |= 1; // Set the current quotient bit to 1
        }
    }

    quotient = q;
    remainder = current_remainder.low; 
} //divideUInt128ByUInt64

struct Int128_t {
    uint64_t high;
    uint64_t low;

    bool is_negative() const {
        return high & (1ULL << 63);
    }
};

inline void divide_u128_by_u64(const UInt128_t & dividend, uint64_t divisor, uint64_t& quotient, uint64_t& remainder)
{
    quotient = 0;
    remainder = 0;

    // Manual bit-by-bit long division
    for (int i = 127; i >= 0; --i) {
        // Shift the current remainder to the left by one bit
        remainder <<= 1;

        // Take the next bit from the dividend and add it to the remainder
        if (i >= 64) {
            if ((dividend.high >> (i - 64)) & 1) {
                remainder |= 1;
            }
        } else {
            if ((dividend.low >> i) & 1) {
                remainder |= 1;
            }
        }

        // If the remainder is now larger than or equal to the divisor
        if (remainder >= divisor) {
            remainder -= divisor;
            // Set the corresponding bit in the quotient
            if (i >= 64) {
                // This means the quotient is larger than 64 bits, which is not expected for this function
                // as it's designed to return a 64-bit quotient.
                // An overflow should be handled here, for simplicity we assume the result fits.
            } else {
                quotient |= (1ULL << i);
            }
        }
    }
}

inline void divide_i128_by_i64(const Int128_t& dividend, int64_t divisor, int64_t& quotient, int64_t& remainder)
{
    // Handle signs
    bool negative_result = dividend.is_negative() != (divisor < 0);
    bool negative_dividend = dividend.is_negative();

    // Convert dividend to its positive equivalent if necessary
    UInt128_t abs_dividend;
    if (negative_dividend) {
        abs_dividend.low = ~dividend.low;
        abs_dividend.high = ~dividend.high;
        if (++abs_dividend.low == 0) {
            ++abs_dividend.high;
        }
    } else {
        abs_dividend.low = dividend.low;
        abs_dividend.high = dividend.high;
    }

    // Convert divisor to its positive equivalent
    uint64_t abs_divisor = divisor > 0 ? divisor : -divisor;

    // Perform the unsigned division
    uint64_t abs_quotient;
    uint64_t abs_remainder;
    divide_u128_by_u64(abs_dividend, abs_divisor, abs_quotient, abs_remainder);

    // Apply the correct signs to the result
    if (negative_result) {
        quotient = -static_cast<int64_t>(abs_quotient);
    } else {
        quotient = static_cast<int64_t>(abs_quotient);
    }

    if (negative_dividend) {
        remainder = -static_cast<int64_t>(abs_remainder);
    } else {
        remainder = static_cast<int64_t>(abs_remainder);
    }
}


