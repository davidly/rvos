#include <stdio.h>

int main( int argc, char * argv[], char * env[] )
{
    for ( int i = 0; i < argc; i++ )
        printf( "argument %d: %s\n", i, argv[ i ] );

    for ( int i = 0; ( 0 != env[ i ]); i++ )
        printf( "environment variable %d: %s\n", i, env[ i ] );
    return argc;
}