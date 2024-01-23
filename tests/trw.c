#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

void show_error( const char * str )
{
    printf( "error: %s, errno: %d == %s\n", str, errno, strerror( errno ) );
    exit( 1 );
}

#define BUF_ELEMENTS 32
static short buf[ BUF_ELEMENTS ];
#define BUF_SIZE ( sizeof( buf ) )

int main()
{
    int i, j, result;
    long int seek_offset, file_offset;

    int fd = open( "trw.dat", O_CREAT | O_RDWR | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO );
    if ( -1 == fd )
        show_error( "unable to create data file" );

    int buf_bytes = sizeof( buf );

    for ( i = 0; i < 4096; i++ )
    {
        for ( j = 0; j < BUF_ELEMENTS; j++ )
            buf[ j ] = i;

        result = write( fd, (char *) buf, buf_bytes );
        if ( buf_bytes != result )
            show_error( "unable to write to file" );
    }

    close( fd );

    fd = open( "trw.dat", O_RDONLY );
    if ( -1 == fd )
        show_error( "unable to open data file read only" );

    for ( i = 0; i < 4096; i++ )
    {
        result = read( fd, (char *) buf, buf_bytes );
        if ( buf_bytes != result )
        {
            printf( "result: %d, i %d\n", result, i );
            show_error( "unable to read from file at point A" );
        }

        for ( j = 0; j < BUF_ELEMENTS; j++ )
            if ( buf[ j ] != i )
                show_error( "data read from file isn't what was expected at point A" );
    }

    close( fd );

    fd = open( "trw.dat", O_RDWR ); //WRONLY );
    if ( -1 == fd )
        show_error( "unable to open data file write only" );

    for ( i = 0; i < 4096; i++ )
    {
        if ( 0 == ( i % 8 ) )
        {
            seek_offset = (long int) i * BUF_SIZE;
            file_offset = lseek( fd, seek_offset, 0 );
            if ( file_offset != seek_offset )
            {
                printf( "file_offset %ld, seek_offset %ld\n", file_offset, seek_offset );
                show_error( "lseek location not as expected" );
            }

            for ( j = 0; j < BUF_ELEMENTS; j++ )
                buf[ j ] = i + 0x4000;

            result = write( fd, (char *) buf, buf_bytes );
            if ( buf_bytes != result )
                show_error( "unable to write to file after lseek" );
        }
    }

    close( fd );

    fd = open( "trw.dat", O_RDONLY );
    if ( -1 == fd )
        show_error( "unable to open data file read only" );

    for ( i = 4095; i >= 0; i-- )
    {
        seek_offset = (long int) i * BUF_SIZE;
        file_offset = lseek( fd, seek_offset, 0 );
        if ( file_offset != seek_offset )
        {
            printf( "file_offset %ld, seek_offset %ld\n", file_offset, seek_offset );
            show_error( "lseek location not as expected" );
        }

        result = read( fd, (char *) buf, buf_bytes );
        if ( buf_bytes != result )
            show_error( "unable to write to file after lseek" );

        for ( j = 0; j < BUF_ELEMENTS; j++ )
        {
            if ( 0 == ( i % 8 ) )
            {
                if ( buf[ j ] != i + 0x4000 )
                    show_error( "data read from file isn't what was expected at point B" );
            }
            else
            {
                if ( buf[ j ] != i )
                    show_error( "data read from file isn't what was expected at point C" );
            }
        }
    }

    close( fd );

#if 0
    result = remove( "trw.dat" );
    if ( 0 != result )
        show_error( "can't unlink test file" );
#endif

    printf( "exiting trw\n" );
    return 0;
}

