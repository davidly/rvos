#include <stdio.h>
#include <cstring>
#include <stdlib.h>

char ac[ 4096 ];
char other[ 4096 ];
char zeroes[ 4096 ] = {0};

extern "C" bool trace_instructions( bool trace );

int main( int argc, char * argv[] )
{
    for ( int i = 0; i < sizeof( ac ); i++ )
        ac[ i ] = ( 'a' + ( i % 26 ) );

    printf( "testing strlen\n" );
    for ( int i = 0; i < 1000; i++ )
    {
        int start = ( (unsigned int) rand() % 300 );
        int end = 1 + start + ( (unsigned int) rand() % 3000 );
        int len = end - start;
        char orig = ac[ end ];
        ac[ end ] = 0;
        int slen = strlen( ac + start );

        if ( len != slen )
        {
            printf( "strlen failed: iteration %d, len %d, strlen %d, start %d, end %d\n", i, len, slen, start, end );
            exit( 1 );
        }
        ac[ end ] = orig;
    }

    printf( "testing strchr and strrchr\n" );
    for ( int i = 0; i < 1000; i++ )
    {
        int start = ( (unsigned int) rand() % 300 ); 
        int end = 1 + start + ( (unsigned int) rand() % 70 );
        int len = end - start;
        char orig = ac[ end ];
        ac[ end ] = '!';
        char * pbang = strchr( ac + start, '!' );
        if ( !pbang )
        {
            printf( "strchr failed to find char: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
            exit( 1 );
        }
        if ( pbang != ( ac + end ) )
        {
            printf( "strchr offset incorrect: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
            exit( 1 );
        }
        pbang = strrchr( ac + start, '!' );
        if ( !pbang )
        {
            printf( "strrchr failed to find char: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
            exit( 1 );
        }

        if ( pbang != ( ac + end ) )
        {
            printf( "strrchr offset incorrect: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
            exit( 1 );
        }
        ac[ end ] = orig;
    }

    printf( "testing memcpy and memcmp\n" );
    for ( int i = 0; i < 1000; i++ )
    {
        int start = ( (unsigned int) rand() % 300 );
        int end = 1 + start + ( (unsigned int) rand() % 3000 );
        int len = end - start;

        memcpy( other + start, ac + start, len );
        if ( memcmp( other + start, ac + start, len ) )
        {
            printf( "memcmp of memcpy'ed memory failed to find match, iteration %d, len %d, start %d, end %d\n", i, len, start, end );
            exit( 1 );
        }
        memset( other + start, 0, len );
        if ( memcmp( other + start, zeroes + start, len ) )
        {
            printf( "zeroes not found in zero-filled memory, iteration %d, len %d, start %d, end %d\n", i, len, start, end );
            exit( 1 );
        }
    }

    printf( "testing printf\n" );
    for ( int i = 0; i < 20; i++ )
    {
        int start = ( (unsigned int) rand() % 300 );
        int end = 1 + start + ( ( (unsigned int) rand() ) % 70 );
        int len = end - start;
        char orig = ac[ end ];
        ac[ end ] = 0;

        int l = strlen( ac + start );
        printf( "%2d (%2d): %s\n", len, l, ac + start );
        ac[ end ] = orig;
    }

    printf( "tstr completed with great success\n" );
    return 0;
}