// tests cases for rvos.
// requires a debug build of rvos since these checks aren't in release builds.

#include <cstring>
#include <stdint.h>
#include <stdio.h>

#include <rvos.h>

#pragma GCC optimize ("O0")

// extra code so the compiler doesn't optimize away the behavior

void recurse( uint64_t depth, const char * pc )
{
    if ( 1000 == depth )
        printf( "why haven't we crashed?\n" );

    char acbuf[ 1024 ];
    sprintf( acbuf, "depth is %llu\n", depth );
    recurse( depth + 1, acbuf );
} //recurse

typedef char pfunc_t( void );

int main( int argc, char * argv[] )
{
    bool ml = true;
    bool mh = false;
    bool pcl = false;
    bool pch = false;
    bool spl = false;
    bool sph = false;
    bool spm = false;

    if ( 2 == argc )
    {
        if ( !strcmp( argv[1], "ml" ) )
            ml = true;
        else if ( !strcmp( argv[1], "mh" ) )
            mh = true;
        else if ( !strcmp( argv[1], "pcl" ) )
            pcl = true;
        else if ( !strcmp( argv[1], "pch" ) )
            pch = true;
        else if ( !strcmp( argv[1], "spl" ) )
            spl = true;
        else if ( !strcmp( argv[1], "sph" ) )
            sph = true;
        else if ( !strcmp( argv[1], "spm" ) )
            spm = true;
    }

    if ( mh ) // memory high
    {
        char * pbad = (char *) 0x2000000000000000;
        *pbad = 10;
    }

    if ( pcl ) // program counter low
    {
        pfunc_t * pf = (pfunc_t *) 0x200;
        int x = (*pf)();
    }

    if ( pch ) // program counter high
    {
        pfunc_t * pf = (pfunc_t *) 0x2000000000000000;
        int x = (*pf)();
    }

    if ( spl ) // stack pointer low
        recurse( 1, "hello" );

    if ( sph ) // stack pointer high
        rvos_sp_add( 0x100000 );

    if ( spm ) // stack pointer misaligned
        rvos_sp_add( 2 );

    if ( ml ) // memory low
    {
        char * pbad = (char *) 0x200;
        *pbad = 10;
    }

    printf( "no crash in tcrash\n" );
    return 0;
} //main

