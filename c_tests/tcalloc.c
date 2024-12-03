#include <stdio.h>
#include <cstring>
#include <stdlib.h>

void chkmem( char * p, int v, int c )
{
    unsigned char * pc = (unsigned char *) p;
    unsigned char val = (unsigned char) ( v & 0xff );
    int i;

    if ( 0 == p )
    {
        printf( "request to chkmem a null pointer\n" );
        exit( 1 );
    }

    for ( i = 0; i < c; i++ )
    {
        if ( *pc != val )
        {
            printf( "memory isn't as expected! p %p i %d, of count %d, val expected %#x, val found %#x\n", 
                    p, i, c, (unsigned char) v, (unsigned char) *pc );

            exit( 1 );
        }
        pc++;
    }
}

int main( int argc, char * argv[] )
{
    for ( int i = 0; i < 1000; i++ )
    {
        int count = 1 + ( rand() % 255 );
        char * p = (char *) calloc( count, 1 );
        chkmem( p, 0, count);
    }

    printf( "tcalloc completed with great success\n" );
    return 0;
}