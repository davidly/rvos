#include <stdio.h>
#include <cstring>
#include <stdlib.h>

char ac[ 4096 ];

int main( int argc, char * argv[] )
{

    for ( int i = 0; i < sizeof( ac ); i++ )
        ac[ i ] = ( 'a' + ( i % 26 ) );

    for ( int i = 0; i < 1000; i++ )
    {
        int start = ( (unsigned int) rand() % 300 );
        int end = 1 + start + ( (unsigned int) rand() % 3000 );
        int len = end - start;
        ac[ end ] = 0;
        int slen = strlen( ac + start );

        if ( len != slen )
        {
            printf( "iteration %d, len %d, strlen %d, start %d, end %d\n", i, len, slen, start, end );
            exit( 1 );
        }
        ac[ end ] = 'E';
    }

    printf( "tstrlen completed with great success\n" );
    return 0;
}