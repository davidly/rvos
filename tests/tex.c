#include <stdio.h>
#include <exception>

using namespace std;

struct exceptional : std::exception
{
    const char * what() const noexcept { return "exceptional"; }
};

int main()
{
    printf( "top of tex\n" );

    try
    {
        throw exceptional();
    }
    catch ( exception & e )
    {
        printf( "caught exception %s\n", e.what() );
    }

    try
    {
        printf( "attempting large allocations\n" );
        for ( size_t i = 0; i < 1000; i++ )
        {
            int * myarray = new int[ 1000000 ];
            printf( "allocation %zd succeeded %p\n", i, myarray );
        }
        printf( "large allocations succeeded?!?\n" );
    }
    catch ( exception & e )
    {
        printf( "caught a standard execption: %s\n", e.what() );
        fflush( stdout );
    }
    catch ( ... )
    {
        printf( "caught generic exception\n" ); 
    }

    printf( "leaving main\n" );
    return 0;
}

