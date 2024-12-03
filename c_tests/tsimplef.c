#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>

int main( int argc, char * argv[] )
{
    //syscall( 0x2002, 1 );
    float f = 3.1415927;
    printf( "float %f\n", f );

    float f1 = 2.5f;
    float f2 = 3.5f;
    float f_mul = f1 * f2;
    float f_div = f1 / f2;
    float f_add = f1 + f2;
    float f_sub = f1 - f2;
    printf( "f1 %f * f2 %f = %f\n", f1, f2, f_mul );
    printf( "    div = %f\n", f_div );
    printf( "    add = %f\n", f_add );
    printf( "    sub = %f\n", f_sub );

    double d1 = 9.72;
    double d2 = 13.321;
    double d_mul = d1 * d2;
    double d_div = d1 / d2;
    double d_add = d1 + d2;
    double d_sub = d1 - d2;
    printf( "d1 %lf * d2 %lf = %lf\n", d1, d2, d_mul );
    printf( "    div = %lf\n", d_div );
    printf( "    add = %lf\n", d_add );
    printf( "    sub = %lf\n", d_sub );

    unsigned __int128 i128 = 1000;
    double d = (double) i128;
    printf( "double from int128: %lf\n", d );

    printf( "tsimplef completed with great success\n");    
    return 0;
}
