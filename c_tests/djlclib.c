// a tiny, crappy subset of the C runtime to avoid the need for clib for testing purposes

#include <djlclib.h>

#define isdigit(d) ((d) >= '0' && (d) <= '9')
#define Ctod(c) ((c) - '0')

#define MAXBUF ( sizeof( uint64_t ) * 8 )  // enough for binary

char * strerror( int err )
{
    return (char *) "unknown error";
}

int fflush( FILE * stream ) { return 0; }

size_t strlen( const char * p )
{
    int len = 0;
    while ( *p )
    {
        len++;
        p++;
    }

    return len;
} //strlen

char * strchr( const char * p, int character )
{
    char c = (char) character;
    while ( *p )
    {
        if ( *p == c )
            return (char *) p;
        p++;
    }
    return 0;
} //strchr

char * strrchr( const char * p, int character )
{
    char c = (char) character;
    int len = strlen( p );
    if ( 0 == len )
        return 0;

    const char * pcur = p + len - 1;
    while ( pcur >= p )
    {
        if ( *pcur == c )
            return (char *) pcur;
        pcur--;
    }
    return 0;
} //strrchr

// disable optimizations above O1 or g++ will have memset recursively call itself as a failed optimization
#pragma GCC push_options
#pragma GCC optimize ("O1")
void * memset( void * ptr, int value, size_t num )
{
    uint8_t * p = (uint8_t *) ptr;
    uint8_t val = (uint8_t) ( value & 0xff );
    for ( size_t i = 0; i < num; i++ )
        p[ i ] = val;
    return ptr;
}
#pragma GCC pop_options

void * memcpy( void * dest, const void * src, size_t count )
{
    uint8_t * d = (uint8_t *) dest;
    uint8_t * s = (uint8_t *) src;
    for ( size_t i = 0; i < count; i++ )
        d[ i ] = s[ i ];
    return dest;
} //memcpy

int memcmp( const void * lhs, const void * rhs, size_t count )
{
    const uint8_t * l = (const uint8_t *) lhs;
    const uint8_t * r = (const uint8_t *) rhs;
    for ( size_t i = 0; i < count; i++ )
    {
        if ( l[ i ] != r[ i ] )
            return (int) (int8_t) l[ i ] - (int) (int8_t) r[ i ];
    }

    return 0;
} //memcmp

// insecure, bad, fast implementation

static int seed = 1;
int rand() 
{
    seed = seed * 1103515245 + 12345;
    seed /= 65536;
    return seed & 0xffff;
} //rand

int puts( const char * str )
{
    return printf( "%s\n", str );
} //puts

#ifdef GPP
extern "C" {
#endif    
int errno = 0;
int * __errno_location( void ) { return &errno; }
void __stack_chk_guard() {}
void __stack_chk_fail() { exit( 99 ); }
#ifdef GPP
}
#endif

int64_t syscall_wrapper()
{
    int64_t result = syscall();
    if ( ( result < 0 ) && ( result > -4096 ) )
    {
        errno = (int) -result;
        return -1;
    }
    return result;
} //syscall_wrapper

int open( const char * pathname, int flags, ... )
{
    int mode = 0;

    if ( 0 != ( flags & O_CREAT ) )
    {
        va_list ap;
        va_start( ap, flags );
        mode = va_arg( ap, int );
        va_end( ap );
    }

    return openat( AT_FDCWD, pathname, flags, mode );
} //open

static void printnum( uint64_t u,   /* number to print */
               int  base,
               void (*putc)(char))
{
    char buf[MAXBUF];
    char * p = &buf[MAXBUF-1];
    static char digs[] = "0123456789abcdef";

    do
    {
        *p-- = digs[u % base];
        u /= base;
    } while (u != 0);

    while (++p != &buf[MAXBUF])
        (*putc)(*p);
} //printnum

int putchar( int ch )
{
    int result = write( 1, &ch, 1 );
    return ch;
}

static void printfloat( float f, int precision, void (*putc)(char) )
{
    if ( f < 0.0 )
    {
        (*putc)( '-' );
        f *= -1.0;
    }
    
    long wholePart = (long) f;
    printnum( wholePart, 10, putc );

    if ( precision > 0 )
    {
        (*putc)( '.' );
        float fraction = f - wholePart;

        while( precision > 0 )
        {
            fraction *= 10;
            wholePart = (long) fraction;
            (*putc)( '0' + wholePart );

            fraction -= wholePart;
            precision--;
        }
    }
} //printfloat

void printdouble( double d, int precision, void (*putc)(char) )
{
    if ( d < 0.0 )
    {
        (*putc)( '-' );
        d *= -1.0;
    }
    
    long wholePart = (long) d;
    printnum( wholePart, 10, putc );

    if ( precision > 0 )
    {
        (*putc)( '.' );
        double fraction = d - wholePart;

        while( precision > 0 )
        {
            fraction *= 10;
            wholePart = (long) fraction;
            (*putc)( '0' + wholePart );

            fraction -= wholePart;
            precision--;
        }
    }
} //printdouble

const static bool _doprnt_truncates = false;

static void _doprnt(
        const char     *fmt,
        va_list        *argp,
        void           (*putc)(char),
        int            radix)          /* default radix - for '%r' */
{
        int      length;
        int      prec;
        bool     ladjust;
        char     padc;
        int64_t  n;
        uint64_t u;
        int      plus_sign;
        int      sign_char;
        bool     altfmt, truncate;
        int      base;
        char     c;
        int      capitals;
        int      num_width = 4;

        while ((c = *fmt) != '\0') {
            if (c != '%') {
                (*putc)(c);
                fmt++;
                continue;
            }

            fmt++;

            length = 0;
            prec = -1;
            ladjust = false;
            padc = ' ';
            plus_sign = 0;
            sign_char = 0;
            altfmt = false;

            while (true) {
                c = *fmt;
                if (c == '#') {
                    altfmt = true;
                }
                else if (c == '-') {
                    ladjust = true;
                }
                else if (c == '+') {
                    plus_sign = '+';
                }
                else if (c == ' ') {
                    if (plus_sign == 0)
                        plus_sign = ' ';
                }
                else
                    break;
                fmt++;
            }

            if (c == '0') {
                padc = '0';
                c = *++fmt;
            }

            if (isdigit(c)) {
                while(isdigit(c)) {
                    length = 10 * length + Ctod(c);
                    c = *++fmt;
                }
            }
            else if (c == '*') {
                length = va_arg(*argp, int);
                c = *++fmt;
                if (length < 0) {
                    ladjust = !ladjust;
                    length = -length;
                }
            }

            if (c == '.') {
                c = *++fmt;
                if (isdigit(c)) {
                    prec = 0;
                    while(isdigit(c)) {
                        prec = 10 * prec + Ctod(c);
                        c = *++fmt;
                    }
                }
                else if (c == '*') {
                    prec = va_arg(*argp, int);
                    c = *++fmt;
                }
            }

            if (c == 'l')
            {
                c = *++fmt;     /* need it if sizeof(int) < sizeof(long) */
                if ( c == 'l' )
                {
                    c = *++fmt;
                    num_width = 8;
                }
            }

            truncate = false;
            capitals=0;         /* Assume lower case printing */

            switch(c) {
                case 'b':
                case 'B':
                {
                    char *p;
                    bool     any;
                    int  i;

                    if ( 4 == num_width )
                        u = va_arg(*argp, uint32_t );
                    else
                        u = va_arg(*argp, uint64_t );
                    p = va_arg(*argp, char *);
                    base = *p++;
                    printnum(u, base, putc);

                    if (u == 0)
                        break;

                    any = false;
                    while ((i = *p++) != '\0') {
                        if (*fmt == 'B')
                            i = 33 - i;
                        if (*p <= 32) {
                            /*
                             * Bit field
                             */
                            int j;
                            if (any)
                                (*putc)(',');
                            else {
                                (*putc)('<');
                                any = true;
                            }
                            j = *p++;
                            if (*fmt == 'B')
                                j = 32 - j;
                            for (; (c = *p) > 32; p++)
                                (*putc)(c);
                            printnum((unsigned)( (u>>(j-1)) & ((2<<(i-j))-1)),
                                        base, putc);
                        }
                        else if (u & (1<<(i-1))) {
                            if (any)
                                (*putc)(',');
                            else {
                                (*putc)('<');
                                any = true;
                            }
                            for (; (c = *p) > 32; p++)
                                (*putc)(c);
                        }
                        else {
                            for (; *p > 32; p++)
                                continue;
                        }
                    }
                    if (any)
                        (*putc)('>');
                    break;
                }

                case 'c':
                    c = va_arg(*argp, int);
                    (*putc)(c);
                    break;

                case 's':
                {
                    char *p;
                    char *p2;

                    if (prec == -1)
                        prec = 0x7fffffff;      /* MAXINT */

                    p = va_arg(*argp, char *);

                    if (p == (char *)0)
                        p = (char *) "";

                    if (length > 0 && !ladjust) {
                        n = 0;
                        p2 = p;

                        for (; *p != '\0' && n < prec; p++)
                            n++;

                        p = p2;

                        while (n < length) {
                            (*putc)(' ');
                            n++;
                        }
                    }

                    n = 0;

                    while (*p != '\0') {
                        if (++n > prec || (length > 0 && n > length))
                            break;

                        (*putc)(*p++);
                    }

                    if (n < length && ladjust) {
                        while (n < length) {
                            (*putc)(' ');
                            n++;
                        }
                    }

                    break;
                }

                case 'o':
                    truncate = _doprnt_truncates;
                case 'O':
                    base = 8;
                    goto print_unsigned;

                case 'd':
                    truncate = _doprnt_truncates;
                case 'D':
                    base = 10;
                    goto print_signed;

                case 'f':
                    goto print_float;

                case 'u':
                    truncate = _doprnt_truncates;
                case 'U':
                    base = 10;
                    goto print_unsigned;

                case 'p':
                    altfmt = true;
                case 'x':
                    truncate = _doprnt_truncates;
                    base = 16;
                    goto print_unsigned;

                case 'X':
                    base = 16;
                    capitals=16;        /* Print in upper case */
                    goto print_unsigned;

                case 'z':
                    truncate = _doprnt_truncates;
                    base = 16;
                    goto print_signed;
                        
                case 'Z':
                    base = 16;
                    capitals=16;        /* Print in upper case */
                    goto print_signed;

                case 'r':
                    truncate = _doprnt_truncates;
                case 'R':
                    base = radix;
                    goto print_signed;

                case 'n':
                    truncate = _doprnt_truncates;
                case 'N':
                    base = radix;
                    goto print_unsigned;

                print_signed:
                    if ( 4 == num_width )
                        n = va_arg(*argp, int32_t );
                    else
                        n = va_arg(*argp, int64_t );
                    if (n >= 0) {
                        u = n;
                        sign_char = plus_sign;
                    }
                    else {
                        u = -n;
                        sign_char = '-';
                    }
                    goto print_num;

                print_unsigned:
                    if ( 4 == num_width )
                        u = va_arg(*argp, uint32_t );
                    else
                        u = va_arg(*argp, uint64_t );
                    goto print_num;

                print_num:
                {
                    char        buf[MAXBUF];    /* build number here */
                    char *     p = &buf[MAXBUF-1];
                    static char digits[] = "0123456789abcdef0123456789ABCDEF";
                    char *prefix = 0;

                    if (truncate) u = (long)((int)(u));

                    if (u != 0 && altfmt) {
                        if (base == 8)
                            prefix = (char *) "0";
                        else if (base == 16)
                            prefix = (char *) "0x";
                    }

                    do {
                        /* Print in the correct case */
                        *p-- = digits[(u % base)+capitals];
                        u /= base;
                    } while (u != 0);

                    length -= (&buf[MAXBUF-1] - p);
                    if (sign_char)
                        length--;
                    if (prefix)
                        length -= strlen((const char *) prefix);

                    if (padc == ' ' && !ladjust) {
                        /* blank padding goes before prefix */
                        while (--length >= 0)
                            (*putc)(' ');
                    }
                    if (sign_char)
                        (*putc)(sign_char);
                    if (prefix)
                        while (*prefix)
                            (*putc)(*prefix++);
                    if (padc == '0') {
                        /* zero padding goes after sign and prefix */
                        while (--length >= 0)
                            (*putc)('0');
                    }
                    while (++p != &buf[MAXBUF])
                        (*putc)(*p);

                    if (ladjust) {
                        while (--length >= 0)
                            (*putc)(' ');
                    }
                    break;
                }
                print_float: // only 4-byte floats are supported
                {
                    // varargs promotes floats to doubles in va_arg

                    double d = va_arg( *argp, double );
                    printdouble( d, ( -1 == prec ? 6 : prec ), putc );
                    break;
                }

                case '\0':
                    fmt--;
                    break;

                default:
                    (*putc)(c);
            }
        fmt++;
        }
} //_doprnt

static int s_count = 0;

static void conslog_putc( char c)
{
    static char ac[2] = {0};
    s_count++;
    ac[0] = c;
    write( 1, ac, 1 );
}

extern int printf( const char *fmt, ... )
{
    s_count = 0;
    va_list listp;
    va_start(listp, fmt);
    _doprnt(fmt, &listp, conslog_putc, 16);
    va_end(listp);
    return s_count;
} //printf

static char *copybyte_str;

static void copybyte( char byte )
{
  *copybyte_str++ = byte;
  *copybyte_str = '\0';
} //copybyte

extern int sprintf( char *buf, const char *fmt, ... )
{
    va_list listp;
    va_start(listp, fmt);
    copybyte_str = buf;
    _doprnt(fmt, &listp, copybyte, 16);
    va_end(listp);
    return strlen(buf);
} //sprintf

extern char * floattoa( char * buffer, float f, int precision )
{
    copybyte_str = buffer;
    printfloat( f, 6, copybyte );
    return buffer;
} //floattoa

