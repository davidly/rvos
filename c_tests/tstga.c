#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>
#include <sys/ioctl.h>
#include <sys/types.h>

int main( int argc, char * argv[] )
{
    struct termios t = { 0 };
    printf( "sizeof termios: %d\n", (int) sizeof( t ) );

    memset( &t, 0, sizeof( t ) );
    int result = tcgetattr( 1, &t );
    printf( "result of tcgetattr for stdout: %d\n", result );
    printf( "iflag: %#x\n", t.c_iflag );
    printf( "oflag: %#x\n", t.c_oflag );
    printf( "cflag: %#x\n", t.c_cflag );
    printf( "lflag: %#x\n", t.c_lflag );
    printf( "c_line %#x\n", t.c_line );

    memset( &t, 0, sizeof( t ) );
    result = tcgetattr( 0, &t );
    printf( "result of tcgetattr for stdin: %d\n", result );
    printf( "iflag: %#x\n", t.c_iflag );
    printf( "oflag: %#x\n", t.c_oflag );
    printf( "cflag: %#x\n", t.c_cflag );
    printf( "lflag: %#x\n", t.c_lflag );
    printf( "c_line %#x\n", t.c_line );

    printf( "exiting test of tcgetattr\n" );
    return 0;
}