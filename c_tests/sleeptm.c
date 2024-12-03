#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <cerrno>
#include <sys/times.h>
#include <sys/resource.h>

#pragma GCC diagnostic ignored "-Wformat="

int main( int argc, char * argv[] )
{
    printf( "this test should take about 3 seconds to complete\n" );
    uint64_t clk_tck = sysconf( _SC_CLK_TCK );
    printf( "clk_tck / number of linux ticks per second: %llu\n", clk_tck );

    struct tms tstart;
    clock_t cstart = times( &tstart );
    struct timespec remaining, request = { 2, 0 };
    int result = nanosleep( &request, &remaining );
    if ( -1 == result )
    {
        printf( "nanosleep failed with error %d\n", errno );
        exit( 1 );
    }

    struct tms tend_sleep;
    clock_t cend_sleep = times( &tend_sleep );
    clock_t cduration = cend_sleep - cstart;
//    printf( "sleep duration %#llx = end %#llx - start %#llx\n", cduration, cend_sleep, cstart );
    printf( "sleep duration %#llx\n", cduration );
    printf( "  sleep duration in milliseconds: %llu\n", ( cduration * 1000 ) / clk_tck );
    printf( "  user time: %llu, kernel time: %llu\n", tend_sleep.tms_utime, tend_sleep.tms_stime );

    uint64_t busy_work = 0;
    do
    {
        clock_t cbusy_loop = times( 0 );
        clock_t busy_time = ( ( cbusy_loop - cend_sleep ) * 1000 ) / clk_tck;
        if ( busy_time >= 1000 )
            break;
        busy_work *= busy_time;
        busy_work -= 33;
        busy_work *= 14;
    } while( true );

    struct tms tend;
    clock_t cend = times( &tend );
    cduration = cend - cend_sleep;
    printf( "busy_work value (this will be somewhat random): %#llx\n", busy_work );
    //printf( "  busy duration %#llx = end %#llx - start %#llx\n", cduration, cend, cend_sleep );
    printf( "  busy duration %#llx\n", cduration );
    printf( "  busy duration in milliseconds: %llu\n", ( cduration * 1000 ) / clk_tck );
    uint64_t user_ms = ( tend.tms_utime * 1000 ) / clk_tck;
    uint64_t system_ms = ( tend.tms_stime * 1000 ) / clk_tck;
    printf( "  user time in ms: %llu, kernel time: %llu\n", user_ms, system_ms );

    struct rusage usage;
    result = getrusage( RUSAGE_SELF, &usage );
    if ( -1 == result )
    {
        printf( "getrusage failed with error %d\n", errno );
        exit( 1 );
    }

    user_ms = ( usage.ru_utime.tv_sec * 1000 ) + ( usage.ru_utime.tv_usec / 1000 );
    system_ms = ( usage.ru_stime.tv_sec * 1000 ) + ( usage.ru_stime.tv_usec / 1000 );
    printf( "getrusage user time in ms: %llu, system time %llu\n", user_ms, system_ms );

    printf( "sleepy time ended with great success\n" );
    return 0;
} //main
