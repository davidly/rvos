#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <search.h>
#include <limits.h>
#include <memory.h>
#include <malloc.h>
#include <vector>

using namespace std;

static uint32_t s_ulColumn = 0;
static bool s_fNumeric = false;
static bool s_fIgnoreCase = false;
static bool s_fReverse = false;
static bool s_fLineLen = false;
static bool s_fUnique = false;

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

int straightcompare( const void * pp1, const void * pp2 )
{
    const char *p1 = * (char **) pp1;
    const char *p2 = * (char **) pp2;

    while ( ( *p1 == *p2 ) &&
            ( *p1 != 13 ) &&
            ( *p2 != 13 ) )
    {
        p1++;
        p2++;
    }
    
    return *p1 - *p2;
}

int docompare( const void *pp1, const void *pp2)
{
    const char *p1,*p2;
    uint32_t ul;
    int i1,i2;
  
    if (s_fReverse)
    {
        p2 = * (char **) pp1;
        p1 = * (char **) pp2;
    }
    else
    {
        p1 = * (char **) pp1;
        p2 = * (char **) pp2;
    }

    if (s_fLineLen)
    {
        while ((*p1 != 13) && (*p2 != 13))
        {
            p1++;
            p2++;
        }
        return *p2 == 13 ? 1 : -1;
    }
    else
    {
        for (ul = 0; (ul < s_ulColumn) && (*p1 != 13) && (*p2 != 13); ul++)
        {
            p1++;
            p2++;
        }
      
        if (s_fNumeric)
        {
            i1 = *p1 == 13 ? INT_MAX : atoi(p1);
            i2 = *p2 == 13 ? INT_MAX : atoi(p2);
      
            return i1 - i2;
        }
        else if (s_fIgnoreCase)
        {
            while ((*p1 != 13) && (*p2 != 13) && (tolower(*p1) == tolower(*p2)))
            {
                p1++;
                p2++;
            }
      
            return tolower(*p1) - tolower(*p2);
        }
        else
        {
            while ((*p1 != 13) && (*p2 != 13) && (*p1 == *p2))
            {
                p1++;
                p2++;
            }
      
            return *p1 - *p2;
        }
    }
} //docompare

int Compare( char **pp1, char **pp2 ) { return docompare(pp1,pp2); }

template<class T> class CSortable
{
public:
    CSortable( T *p, uint32_t c ) :
        _pElems(p),
        _cElems(c) {}

    // sort the array using a heap sort

    void Sort()
    {
        if ( _cElems < 2 )
            return;

        for ( long i = ( ( (long) _cElems + 1 ) / 2 ) - 1; i >= 0; i-- )
        {
            AddRoot( i, _cElems );
        }

        for ( long i = _cElems - 1; 0 != i; i-- )
        {
            Swap( _pElems, _pElems + i );
            AddRoot( 0, i );
        }
    } //Sort

private:

    void Swap( T * p1, T * p2 )
    {
        T tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
    }

    void AddRoot( uint32_t x, uint32_t cItems )
    {
        uint32_t y = ( 2 * ( x + 1 ) ) - 1;
    
        while ( y < cItems )
        {

            if ( ( y + 1 ) < cItems )
            {
                int i = Compare( _pElems + y, _pElems + y + 1 );
                if ( i < 0 )
                    y++;
            }
    
            int i = Compare( _pElems + x, _pElems + y );

            if ( i < 0 )
            {
                Swap( _pElems + x, _pElems + y );
                x = y;
                y = ( 2 * ( y + 1 ) ) - 1;
            }
            else
                break;
        }
    } //AddRoot

    T *         _pElems;
    const uint32_t _cElems;
};

template<class T> class CSortableStraight
{
public:
    CSortableStraight( T *p, uint32_t c ) :
        _pElems(p),
        _cElems(c) {}

    // sort the array using a heap sort

    void Sort()
    {
        if ( _cElems < 2 )
            return;

        for ( long i = ( ( (long) _cElems + 1 ) / 2 ) - 1; i >= 0; i-- )
        {
            AddRoot( i, _cElems );
        }

        for ( long i = _cElems - 1; 0 != i; i-- )
        {
            Swap( _pElems, _pElems + i );
            AddRoot( 0, i );
        }
    } //Sort

private:

    void Swap( T * p1, T * p2 )
    {
        T tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
    }

    void AddRoot( uint32_t x, uint32_t cItems )
    {
        uint32_t y = ( 2 * ( x + 1 ) ) - 1;
    
        while ( y < cItems )
        {

            if ( ( y + 1 ) < cItems )
            {
                int i = straightcompare( (void **) ( _pElems + y ), (void **) ( _pElems + y + 1 ) );
                if ( i < 0 )
                    y++;
            }
    
            int i = straightcompare( (void **) ( _pElems + x ), (void **) ( _pElems + y ) );

            if ( i < 0 )
            {
                Swap( _pElems + x, _pElems + y );
                x = y;
                y = ( 2 * ( y + 1 ) ) - 1;
            }
            else
                break;
        }
    } //AddRoot

    T *         _pElems;
    const uint32_t _cElems;
};

long portable_filelen( FILE * fp )
{
    long current = ftell( fp );
    fseek( fp, 0, SEEK_END );
    long len = ftell( fp );
    fseek( fp, current, SEEK_SET );
    return len;
} //portable_filelen

void Usage(void)
{
    printf("Usage:  mysort [-i] [-c X] [-r] [-n] [-l] infile outfile\n");
    printf("               -l      sort based on line length only\n");
    printf("               -i      ignore case\n");
    printf("               -r      reverse sort\n");
    printf("               -c X    sort starting on column X (0 based)\n");
    printf("               -n      sort numbers, not alphanumerics\n");
    printf("               -q      use quicksort, not heapsort\n");
    printf("               -u      string-wise unique-ify the output\n");

    exit(1);
} //Usage

int main(int argc,char *argv[])
{
    char **asz,*pc,*pcIn=NULL,*pcOut=NULL;
    uint32_t ulLineCount,ulNum,i;
    bool use_quicksort = false;
    FILE *fpOut, *fpIn;
  
    for ( int j = 1; j < argc; j++ )
    {
        strlwr(argv[j]);

        if (argv[j][0] == '-' || argv[j][0] == '/')
        {
            if (argv[j][1] == 'i')
                s_fIgnoreCase = true;
            else if (argv[j][1] == 'r')
                s_fReverse = true;
            else if (argv[j][1] == 'u')
                s_fUnique = true;
            else if (argv[j][1] == 'n')
                s_fNumeric = true;
            else if (argv[j][1] == 'q')
                use_quicksort = true;
            else if (argv[j][1] == 'l')
                s_fLineLen = true;
            else if (argv[j][1] == 'c')
            {
                j++;
                if (j < argc)
                {
                    s_ulColumn = (uint32_t) __max(0,atoi(argv[j]) - 1);
                }
                else Usage();
            }
            else Usage();
        }
        else
        {
            if (pcIn)
                pcOut = argv[j];
            else
                pcIn = argv[j];
        }
    }
  
    if (pcIn && pcOut &&
        (fpIn = fopen(pcIn,"rb")) &&
        (fpOut = fopen(pcOut,"wb")))
    {
        printf("sorting %s ==> %s\n",pcIn,pcOut);

        long filelen = portable_filelen( fpIn );
        vector<char> input( 1 + filelen );
        long lread = fread( input.data(), 1, filelen, fpIn );
        input[ filelen ] = 0;
        ulLineCount = 0;
        fclose( fpIn );

        for ( long i = 0; i < filelen; i++ )
        {
            if ( 10 == input[ i ] )
                ulLineCount++;
        }

        if ( asz = (char **) malloc( ulLineCount * sizeof(char *) ) )
        {
            ulNum = 0;
            pc = input.data();
      
            while (ulNum < ulLineCount)
            {
                asz[ulNum++] = pc;
      
                while (*pc != 10)
                    pc++;
  
                pc++;
            }

            printf( "sorting\n" );

            if ( ( 0 == s_ulColumn ) &&
                 ( !s_fNumeric ) &&
                 ( !s_fIgnoreCase ) &&
                 ( !s_fReverse ) &&
                 ( !s_fLineLen ) )
            {
                if ( use_quicksort )
                {
                    qsort( asz, ulLineCount, sizeof(char *), straightcompare );
                }
                else
                {
                    CSortableStraight<char *> sort( asz, ulLineCount );
                    sort.Sort();
                }
            }
            else
            {
                if ( use_quicksort )
                {
                    qsort( asz, ulLineCount, sizeof(char *), docompare );
                }
                else
                {
                    CSortable<char *> sort( asz, ulLineCount );
                    sort.Sort();
                }
            }

            printf( "writing...\n" );

            if ( s_fUnique )
            {
                const char *pcPrev = "xyzzyunique\r\n";
    
                for (i = 0; i < ulLineCount; i++)
                {
                    pc = asz[i];
    
                    if ( 0 != straightcompare( (void **) &pc, (void **) &pcPrev ) )
                    {
                        while (*pc != 10)
                        {
                            fputc(*pc,fpOut);
                            pc++;
                        }
              
                        fputc(*pc,fpOut);
                    }
    
                    pcPrev = asz[i];
                }
            }
            else
            {
                for (i = 0; i < ulLineCount; i++)
                {
                    pc = asz[i];
    
                    while (*pc != 10)
                    {
                        fputc(*pc,fpOut);
                        pc++;
                    }
              
                    fputc(*pc,fpOut);
                }
            }

            free( asz );
            printf( "done\n" );
        }
        else
        {
            printf("Can't allocate enough memory!\n");
        }
  
        fclose(fpOut);
  
    }
    else Usage();
  
    return 0;
} //main

