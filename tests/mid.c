#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

using namespace std;

#define _countof( X ) ( sizeof( X ) / sizeof( X[0] ) )

void usage( const char * pcError = 0 )
{
    if ( pcError )
        printf( "Error: %s\n", pcError );

    printf( "mid. like head or tail, but...\n" );
    printf( "  usage: mid [-c:X] [-l:Y] file\n" );
    printf( "     -l is the starting line, default 1\n" );
    printf( "     -c is the count of lines to show, default 10\n" );
    printf( "  example: mid /l:4000000 /c:30 massivefile.log\n" );
    exit( 1 );
} //usage

int main( int argc, char * argv[] )
{
    long long count = 10;
    long long first = 1;
    char * pcfile = 0;

    for ( int i = 1; i < argc; i++ )
    {
        char *parg = argv[i];
        char c = *parg;

        if ( '-' == c || '/' == c )
        {
            char ca = (char) tolower( parg[1] );

            if ( 'c' == ca )
            {
                if ( ':' == parg[2] )
                    count = strtoull( parg + 3 , 0, 10 );
                else
                    usage( "colon required after c argument" );
            }
            else if ( 'l' == ca )
            {
                if ( ':' == parg[2] )
                    first = strtoull( parg + 3 , 0, 10 );
                else
                    usage( "colon required after c argument" );
            }
            else if ( '?' == ca )
                usage();
            else
                usage( "invalid argument specified" );
        }
        else
        {
            if ( 0 == pcfile )
                pcfile = parg;
            else
                usage( "too many arguments" );
        }
    }

    if ( 0 == pcfile )
        usage( "no file specified" );

    FILE * fp = fopen( pcfile, "r" );
    if ( !fp )
        usage( "can't open file" );

    static char acA[ 65536 ];
    long long line = 1;

    printf( "starting at line %llu for %llu lines\n", first, count );

    do
    {
        char * pca = fgets( acA, _countof( acA ), fp );

        if ( !pca )
        {
            printf( "out of lines in the file; looked at %llu\n", line );
            fclose( fp );
            exit( 1 );
        }

        if ( line >= first )
            printf( "%llu: %s", line, acA );

        line++;

        if ( line > ( first + count ) )
            break;
    } while( true );

    fclose( fp );

    return 0;
} //main

