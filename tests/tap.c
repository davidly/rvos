#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>

template <class T> T __max( T a, T b )
{
    if ( a > b )
        return a;
    return b;
}

template <class T> T __min( T a, T b )
{
    if ( a < b )
        return a;
    return b;
}

int gcd( int m, int n )
{
    int a = 0;
    int b = __max( m, n );
    int r = __min( m, n );

    while ( 0 != r )
    {
        a = b;
        b = r;
        r = a % b;
    }

    return b;
} //gcd

int randi()
{
    //return rvos_rand() & 0x7fffffff;
    return rand();
} //randi

// https://en.wikipedia.org/wiki/Ap%C3%A9ry%27s_theorem

void first_implementation()
{
    // use 128 bit ints for testing but also because the loop can go futher without integer overflow

    const unsigned __int128 total = 100000; //00;
    double sofar = 0;
    unsigned __int128 prev = 1;

    for ( unsigned __int128 i = 1; i <= total; i++ )
    {
        sofar += (double) 1.0 / (double) ( i * i * i );

        if ( i == ( prev * 10 ) )
        {
            prev = i;
            printf( "  at %12llu iterations: %.20lf\n", (uint64_t) i, sofar );
        }
    }
} //first_implementation

int main()
{
    printf( "starting, should tend towards 1.2020569031595942854...\n" );

    first_implementation();

    printf( "next implementation...\n" );

    const int totalEntries = 100000; //00;
    int totalCoprimes = 0;

    int prev = 1;

    for ( int i = 1; i <= totalEntries; i++ )
    {
        int a = randi();
        int b = randi();
        int c = randi();

        int greatest = gcd( a, gcd( b, c ) );
        if ( 1 == greatest )
            totalCoprimes++;

        if ( i == ( prev * 10 ) )
        {
            prev = i;
            printf( "  at %12d iterations: %.20lf\n", i, (double) i / (double) totalCoprimes );
        }
    }

    printf( "done\n" );
    exit( 1202 );
    return 1202;
} //main
