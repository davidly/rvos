#include <stdio.h>

int main( int argc, char * argv[] )
{
    char acbuf[ 100 ];
    char * result = fgets( acbuf, sizeof( acbuf ), stdin );
    if ( 0 == result )
        printf( "gets failed\n" );
    else
    {
        while ( *result )
        {
            printf( "read: '%c' == %u == %#x\n", *result, *result, *result );
            result++;
        }
    }
    printf( "tgets completed with great success\n" );
    return 0;
}
