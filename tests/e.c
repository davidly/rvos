#include <string.h>
#include <stdio.h>
#include <stdlib.h>

//#pragma GCC optimize ("O0")

void swap( char & a, char & b )
{
    char c = a;
    a = b;
    b = c;
} //swap

void reverse( char str[], int length )
{
    int start = 0;
    int end = length - 1;
    while ( start < end )
    {
        swap( * ( str + start ), * ( str + end ) );
        start++;
        end--;
    }
} //reverse
 
char * i64toa( int64_t num, char * str, int base )
{
    int i = 0;
    bool isNegative = false;
 
    if ( 0 == num )
    {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }
 
    if ( num < 0 && 10 == base )
    {
        isNegative = true;
        num = -num;
    }
 
    while ( 0 != num )
    {
        int rem = num % base;
        str[i++] = (rem > 9)? (rem-10) + 'a' : rem + '0';
        num = num/base;
    }
 
    if (isNegative)
        str[i++] = '-';
 
    str[i] = '\0';
 
    reverse( str, i );
 
    return str;
} //i64toa


extern "C" void riscv_print_text( const char * p );

extern "C" int main() {
  char buf[ 128 ];
  int N = 9009, x = 0;
  int a[9009];
  for (int n = N - 1; n > 0; --n) {
          if ( n < 0 )
              riscv_print_text( "n is less than 0!!!\n" );
      a[n] = 1;
  }

  riscv_print_text( "after initial loop\n" );
  a[1] = 2, a[0] = 0;
  while (N > 9) {
      int n = N--;
      while (--n) {
          if ( n < 1 )
              riscv_print_text( "n is less than 1!!!\n" );

          if ( n >= 9009 )
              riscv_print_text( "n is too large\n" );

          a[n] = x % n;

          x = 10 * a[n-1] + x/n;
      }
      i64toa( x, buf, 10 );
      riscv_print_text( buf );
      //printf("%d", x);
  }

  riscv_print_text( "done" );

  return 0;
}
