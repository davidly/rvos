#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <cerrno>
#include <cstring>

int main( int argc, char * argv[] )
{
    DIR * dir = opendir( "." );
    if ( 0 == dir )
    {
        printf( "can't open current folder, error %d\n", errno );
        return -1;
    }

    int count = 0;
    bool parent_found = false;

    do
    {
        struct dirent * entry = readdir( dir );
        if ( !entry )
            break;
        if ( !strcmp( "..", entry->d_name ) )
            parent_found = true;

        count++;
    } while( true );

    if ( !parent_found )
    {
        printf( "error: parent folder not found in enumeration out of %d files returned\n", count );
        return 1; 
    }

    closedir( dir );

    printf( "linux file system enumeration completed with great success\n" );
    return 0;
} // main