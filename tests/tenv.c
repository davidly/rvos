#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main( int argc, char * argv[], char * envp[] )
{
    if ( 0 != envp )
    {
        for ( int j = 0; ; j++ )
        {
            if ( 0 == envp[ j ] )
                break;
            printf( "environment variable %d: %p == '%s'\n", j, envp[ j ], envp[ j ] );
        }
    }

    bool isRVOS = ( 0 != envp && 0 != envp[ 0 ] && !strcmp( envp[ 0 ], "OS=RVOS" ) );
    printf( "is RVOS: %s\n", isRVOS ? "yes" : "no" );

    char ac[100];
    strcpy( ac, "MYVAL=toast!" );
    char * ptoast = (char *) malloc( strlen( ac ) + 1 );
    strcpy( ptoast, ac );
    putenv( ac );

    char * pval = getenv( "MYVAL" );
    printf( "pval: %p\n", pval );
    if ( 0 != pval )
        printf( "value: %s\n", pval );

    char * posval = getenv( "OS" );
    printf( "OS: %p\n", posval );
    if ( 0 != posval )
        printf( "value: %s\n", posval );

    char * timezoneval = getenv( "TZ" );
    printf( "TZ: %p\n", timezoneval );
    if ( 0 != timezoneval )
        printf( "TZ: '%s'\n", timezoneval );

    return 0;
} //main
