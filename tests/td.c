#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include <rvos.h>

extern "C" int main()
{
    double d = 3.14159265358979323846264338327952884197169399375105820974944592307;

    char ac[100];
    sprintf( ac, "sprintf double %.20f\n", d );
    printf( ac );
exit(1);
    printf( "double from printf: %.20f\n", d );
    rvos_printf( "double: %.20f\n", d );

    float f = 1.2020569;
    rvos_printf( "float: %f\n", f );
    printf( "float from printf: %f\n", f );

    double r = -f * d;
    rvos_printf( "double r: %lf\n", r );
    printf( "double from printf r: %lf\n", r );

    printf( "stop\n" );
    return 0;
} //main
