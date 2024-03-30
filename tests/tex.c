#include <stdio.h>
#include <exception>

using namespace std;

class CUnwound
{
    private:
        int x;
    public:
        CUnwound() : x( 44 ) {}
        ~CUnwound() { printf( "I am unwound, x: %d\n", x ); }
        void set( int val ) { x = val; }
};

struct exceptional : std::exception
{
    const char * what() const noexcept { return "exceptional"; }
};

int main()
{
    printf( "top of tex\n" );

    try
    {
        CUnwound unwound;
        throw exceptional();
        unwound.set( 33 );
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

