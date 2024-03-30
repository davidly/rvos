#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

int main( int argc, char * argv[] )
{
    FILE * fp;
    const char * foldername = "testfolder";

    int result = mkdir( foldername, S_IRWXU | S_ISVTX );
    if ( 0 != result )
    {
        printf( "mkdir failed, error %d\n", errno );
        printf( "folder '%s' exists; deleting it\n", foldername );
        fflush( stdout );
        result = rmdir( foldername );
        if ( 0 != result )
        {
            printf( "start of app cleanup: rmdir of folder failed, error %d\n", errno );
            exit( 1 );
        }

        int result = mkdir( foldername, S_IRWXU | S_ISVTX );
        if ( 0 != result )
        {
            printf( "creation of folder failed, error %d\n", errno );
            exit( 1 );
        }
    }

    result = chdir( foldername );
    if ( 0 != result )
    {
        printf( "cd into the test folder failed, error %d\n", errno );
        exit( 1 );
    }

    fp = fopen( "a-file.txt", "w+" );
    if ( !fp )
    {
        printf( "creation of a-file.txt in new folder failed, error %d\n", errno );
        exit( 1 );
    }

    fprintf( fp, "hello!\n" );
    fclose( fp );

    result = unlink( "a-file.txt" );
    if ( 0 != result )
    {
        printf( "removal of a-file.txt failed, error %d\n", errno );
        exit( 1 );
    }

    result = chdir( ".." );
    if ( 0 != result )
    {
        printf( "cd back up to previous folder .. failed, error %d\n", errno );
        exit( 1 );
    }

    result = rmdir( foldername );
    if ( 0 != result )
    {
        printf( "end of app cleanup: rmdir of folder failed, error %d\n", errno );
        exit( 1 );
    }

    printf( "success\n" );
    return 0;
} //main

