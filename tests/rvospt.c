// compile and link with this for apps like those generated from the BA compiler that call this RVOS APIs
// in order for the apps to run on Linux running on RISC-V (or in the RVOS emulator).

#include <stdio.h>
#include <ctime>
#include <cstdint>
#include <chrono>

using namespace std;
using namespace std::chrono;

extern "C" void rvos_print_text( char * pc );

extern "C" void rvos_get_datetime( char * pc );

void rvos_get_datetime( char * pc )
{
    system_clock::time_point now = system_clock::now();
    uint64_t ms = duration_cast<milliseconds>( now.time_since_epoch() ).count() % 1000;
    time_t time_now = system_clock::to_time_t( now );
    struct tm * plocal = localtime( & time_now );
    sprintf( pc, "%02u:%02u:%02u.%03u", (uint32_t) plocal->tm_hour, (uint32_t) plocal->tm_min, (uint32_t) plocal->tm_sec, (uint32_t) ms );
}

void rvos_print_text( char * pc )
{
    printf( "%s", pc );
}
