#include <stdio.h>
#include <cstring>
#include <stdlib.h>
#include <cwchar>
#include <locale.h>

char ac[ 4096 ];
char other[ 4096 ];
char zeroes[ 4096 ] = {0};

wchar_t wc[ 4096 ];
wchar_t wc_other[ 4096 ];
wchar_t wc_zeroes[ 4096 ] = {0};

#define _countof( X ) ( sizeof( X ) / sizeof( X[0] ) )

extern "C" bool trace_instructions( bool trace );

void test_wide()
{
    for ( int i = 0; i < _countof( wc ); i++ )
        wc[ i ] = ( L'a' + ( i % 26 ) );

    printf( "testing wcslen\n" );
    for ( int i = 0; i < 1000; i++ )
    {
        int start = ( (unsigned int) rand() % 300 );
        int end = 1 + start + ( (unsigned int) rand() % 3000 );
        int len = end - start;
        wchar_t orig = wc[ end ];
        wc[ end ] = 0;
        int slen = wcslen( wc + start );

        if ( len != slen )
        {
            printf( "wcslen failed: iteration %d, len %d, wcslen %d, start %d, end %d\n", i, len, slen, start, end );
            exit( 1 );
        }
        wc[ end ] = orig;
    }

    printf( "testing wcschr and wcsrchr\n" );
    for ( int i = 0; i < 1000; i++ )
    {
        int start = ( (unsigned int) rand() % 300 ); 
        int end = 1 + start + ( (unsigned int) rand() % 70 );
        int len = end - start;
        wchar_t orig = wc[ end ];
        wc[ end ] = L'!';
        wchar_t * pbang = wcschr( wc + start, L'!' );
        if ( !pbang )
        {
            printf( "wcschr failed to find char: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
            exit( 1 );
        }
        if ( pbang != ( wc + end ) )
        {
            printf( "wcschr offset incorrect: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
            exit( 1 );
        }
        pbang = wcsrchr( wc + start, '!' );
        if ( !pbang )
        {
            printf( "wcsrchr failed to find char: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
            exit( 1 );
        }

        if ( pbang != ( wc + end ) )
        {
            printf( "wcsrchr offset incorrect: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
            exit( 1 );
        }
        wc[ end ] = orig;
    }

    printf( "testing wcsstr xxx\n" );
    wchar_t alpha[27];
    wcscpy( alpha, L"abcdefghijklmnopqrstuvwxyz" );
    for ( int i = 0; i < 1000; i++ )
    {
        int start = ( (unsigned int) rand() % 300 );
        int offset = ( (unsigned int) rand() % 26 );
        int len = 1 + ( (unsigned int) rand() % ( 26 - offset ) );
        if ( ( offset + len ) > 26 )
        {
            printf( "test bug offset %d, len %d\n", offset, len );
            exit( 1 );
        }
        const wchar_t * pattern = alpha + offset;
        const wchar_t * p = wcsstr( wc + start, pattern );
        if ( !p )
        {
            printf( "strstr pattern not found iteration %d, start %d, offset %d, len %d, pattern %ls\n", i, start, offset, len, pattern );
            exit( 1 );
        }
        if ( memcmp( p, pattern, len * sizeof( wchar_t ) ) )
        {
            printf( "strstr found the wrong pattern iteration %d, start %d, offset %d, len %d, pattern %ls\n", i, start, offset, len, pattern );
            exit( 1 );
        }
    }
} //test_wide

int main( int argc, char * argv[] )
{
    for ( int i = 0; i < sizeof( ac ); i++ )
        ac[ i ] = ( 'a' + ( i % 26 ) );

    printf( "testing strlen\n" );
    for ( int i = 0; i < 1000; i++ )
    {
        int start = ( (unsigned int) rand() % 300 );
        int end = 1 + start + ( (unsigned int) rand() % 3000 );
        int len = end - start;
        char orig = ac[ end ];
        ac[ end ] = 0;
        int slen = strlen( ac + start );

        if ( len != slen )
        {
            printf( "strlen failed: iteration %d, len %d, strlen %d, start %d, end %d\n", i, len, slen, start, end );
            exit( 1 );
        }
        ac[ end ] = orig;
    }

    printf( "testing strchr and strrchr\n" );
    for ( int i = 0; i < 1000; i++ )
    {
        int start = ( (unsigned int) rand() % 300 ); 
        int end = 1 + start + ( (unsigned int) rand() % 70 );
        int len = end - start;
        char orig = ac[ end ];
        ac[ end ] = '!';
        char * pbang = strchr( ac + start, '!' );
        if ( !pbang )
        {
            printf( "strchr failed to find char: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
            exit( 1 );
        }
        if ( pbang != ( ac + end ) )
        {
            printf( "strchr offset incorrect: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
            exit( 1 );
        }
        pbang = strrchr( ac + start, '!' );
        if ( !pbang )
        {
            printf( "strrchr failed to find char: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
            exit( 1 );
        }

        if ( pbang != ( ac + end ) )
        {
            printf( "strrchr offset incorrect: iteration %d, len %d, start %d, end %d\n", i, len, start, end );
            exit( 1 );
        }
        ac[ end ] = orig;
    }

    printf( "testing strstr\n" );
    char alpha[27];
    strcpy( alpha, "abcdefghijklmnopqrstuvwxyz" );
    for ( int i = 0; i < 1000; i++ )
    {
        int start = ( (unsigned int) rand() % 300 );
        int offset = ( (unsigned int) rand() % 26 );
        int len = 1 + ( (unsigned int) rand() % ( 26 - offset ) );
        if ( ( offset + len ) > 26 )
        {
            printf( "test bug offset %d, len %d\n", offset, len );
            exit( 1 );
        }
        const char * pattern = alpha + offset;
        const char * p = strstr( ac + start, pattern );
        if ( !p )
        {
            printf( "strstr pattern not found iteration %d, start %d, offset %d, len %d, pattern %s\n", i, start, offset, len, pattern );
            exit( 1 );
        }
        if ( memcmp( p, pattern, len ) )
        {
            printf( "strstr found the wrong pattern iteration %d, start %d, offset %d, len %d, pattern %s\n", i, start, offset, len, pattern );
            exit( 1 );
        }
    }

    printf( "testing memcpy and memcmp\n" );
    for ( int i = 0; i < 1000; i++ )
    {
        int start = ( (unsigned int) rand() % 300 );
        int end = 1 + start + ( (unsigned int) rand() % 3000 );
        int len = end - start;

        memcpy( other + start, ac + start, len );
        if ( memcmp( other + start, ac + start, len ) )
        {
            printf( "memcmp of memcpy'ed memory failed to find match, iteration %d, len %d, start %d, end %d\n", i, len, start, end );
            exit( 1 );
        }
        memset( other + start, 0, len );
        if ( memcmp( other + start, zeroes + start, len ) )
        {
            printf( "zeroes not found in zero-filled memory, iteration %d, len %d, start %d, end %d\n", i, len, start, end );
            exit( 1 );
        }
    }

    printf( "testing printf\n" );
    for ( int i = 0; i < 20; i++ )
    {
        int start = ( (unsigned int) rand() % 300 );
        int end = 1 + start + ( ( (unsigned int) rand() ) % 70 );
        int len = end - start;
        char orig = ac[ end ];
        ac[ end ] = 0;

        int l = strlen( ac + start );
        printf( "%2d (%2d): %s\n", len, l, ac + start );
        ac[ end ] = orig;
    }

    test_wide();

    printf( "tstr completed with great success\n" );
    return 0;
} //main
