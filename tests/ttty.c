#include <stdio.h>

#if defined( __GNUC__ ) || defined( __clang__ )
#include <unistd.h>
#else
#include <io.h>
#endif

const char * is_tty( FILE * fp )
{
    return isatty( fileno( fp ) ) ? "yes" : "no";
}

int main()
{
    printf( "is stdin tty:  %s\n", is_tty( stdin ) );
    fflush( stdout );
    printf( "is stdout tty: %s\n", is_tty( stdout ) );
    fflush( stdout );
    return 0;
}

