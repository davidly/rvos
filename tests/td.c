#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#pragma GCC optimize ("O0")

extern "C" int main()
{
    double pi = 3.14159265358979323846264338327952884197169399375105820974944592307;

    char ac[100];
    sprintf( ac, "sprintf double %.20f\n", pi );
    printf( ac );

    printf( "double from printf: %.20f\n", pi );

    float f = 1.2020569;
    printf( "float from printf: %f\n", f );

    double r = -f * pi;
    printf( "double from printf r: %lf\n", r );

    double sq = sqrt( pi );
    printf( "sqrt of pi: %lf\n", sq );

    double radians = pi / 180.0 * 30.0;

    double s = sin( radians );
    printf( "sin of 30 degress is %lf\n", s );

    double c = cos( radians );
    printf( "cos of 30 degrees is %lf\n", c );

    double t = tan( radians );
    printf( "tan of 30 degrees is %lf\n", t );

    int exponent;
    double mantissa = frexp( pi, &exponent );
    printf( "mantissa: %lf, exponent %d\n", mantissa, exponent );

    printf( "stop\n" );
    return 0;
} //main
