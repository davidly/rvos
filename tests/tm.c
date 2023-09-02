#include <memory.h>
#include <stdlib.h>
#include <stdio.h>

#define allocs 69
int logging = 1;

char * memset_x( char * p, int v, int c )
{
    unsigned char * pc = (unsigned char *) p;
    unsigned char val = (unsigned char) ( v & 0xff );
    int i;

    if ( 0 == p )
    {
        printf( "request to memset a null pointer\n" );
        exit( 1 );
    }

    if ( logging )
#ifdef CPMTIME
        printf( "  memset p %u, v %d, val %x, c %d\n", p, v, val, c );
#else
        printf( "  memset p %p, v %d, val %x, c %d\n", p, v, val, c );
#endif

    for ( i = 0; i < c; i++ )
        *pc++ = val;
    return p;
}

void chkmem( char * p, int v, int c )
{
    unsigned char * pc = (unsigned char *) p;
    unsigned char val = (unsigned char) ( v & 0xff );
    int i;

    if ( 0 == p )
    {
        printf( "request to chkmem a null pointer\n" );
        exit( 1 );
    }

    for ( i = 0; i < c; i++ )
    {
        if ( *pc != val )
        {
#ifdef CPMTIME
            printf( "memory isn't as expected! p %u, v %d, c %d, *pc %d\n",p, v, c, *pc );
#else
            printf( "memory isn't as expected! p %p, v %d, c %d, *pc %d\n",p, v, c, *pc );
#endif
            exit( 1 );
        }
        pc++;
    }
}

int main( int argc, char * argv[] )
{
    int i, cb, j;
    char * ap[ allocs ];
    char * pc;

    logging = ( argc > 1 );
    pc = argv[ 0 ]; /* evade compiler warning */

    for ( j = 0; j < 10; j++ )
    {
        if ( logging )
            printf( "in alloc mode\n" );
    
        for ( i = 0; i < allocs; i++ )
        {
            cb = 8 + ( i * 10 );
            if ( logging )
                printf( "  i, cb: %d %d\n", i, cb );
    
            pc = (char *) calloc( cb + 5, 1 );
            memset_x( pc, 0xcc, cb + 5 );
    
            ap[ i ] = (char *) malloc( cb );
            memset_x( ap[ i ], 0xaa, cb );
    
            chkmem( pc, 0xcc, cb );
            free( pc );
        }
    
        if ( logging )
            printf( "in free mode, even first\n" );
    
        for ( i = 0; i < allocs; i += 2 )
        {
            cb = 8 + ( i * 10 );
            if ( logging )
                printf( "  i, cb: %d %d\n", i, cb );
    
            pc = (char *) calloc( cb + 3, 1 );
            memset_x( pc, 0xcc, cb + 3 );
    
            chkmem( ap[ i ], 0xaa, cb );
            memset_x( ap[ i ], 0xff, cb );
            free( ap[ i ] );
    
            chkmem( pc, 0xcc, cb );
            free( pc );
        }
    
        if ( logging )
            printf( "in free mode, now odd\n" );
    
        for ( i = 1; i < allocs; i += 2 )
        {
            cb = 8 + ( i * 10 );
            if ( logging )
                printf( "  i, cb: %d %d\n", i, cb );
    
            pc = (char *) calloc( cb + 7, 1 );
    
            chkmem( ap[ i ], 0xaa, cb );
            memset_x( ap[ i ], 0xff, cb );
            free( ap[ i ] );
    
            chkmem( pc, 0, cb );
            free( pc );
        }
    }

    printf( "success\n" );
    return 0;
}
