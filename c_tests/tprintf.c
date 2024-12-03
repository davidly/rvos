#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define BA_ENABLE_COMPILER

static void Usage()
{
    printf( "Usage: ba [-a] [-e] [-l] [-m] [-p] [-t] [-x] [-8] filename.bas [argvalue]\n" );
    printf( "  Basic interpreter\n" );
    printf( "  Arguments:     filename.bas     Subset of TRS-80 compatible BASIC\n" );

    printf( "                 argvalue         One optional integer argument to the app referenced in basic as av%%\n" );
#ifdef BA_ENABLE_COMPILER
    printf( "                 -a:X             Generate assembly code, where X is one of:\n" );
    printf( "                                  6 -- Generate 8-bit Apple 1 'sbasm30306\\sbasm.py' compatible assembler code to filename.s\n" );
    printf( "                                  8 -- Generate 8-bit CP/M 2.2 i8080 'asm' compatible assembler code to filename.asm\n" );
    printf( "                                  a -- Generate 64-bit arm64 Windows armasm64 compatible assembler code to filename.asm\n" );
    printf( "                                  d -- Generate 16-bit 8086 DOS ml /AT /omf /c compatible assembler code to filename.asm\n" );
    printf( "                                  3 -- Generate 32-bit Linux arm32 armv8 'gcc / as' compatible assembler code to filename.s\n" );
    printf( "                                  i -- Generate 32-bit i386 (686) Windows x86 'ml' compatible assembler code to filename.asm\n" );
    printf( "                                  I -- Generate 32-bit i386 (386) Windows 98 'ml' compatible assembler code to filename.asm\n" );
    printf( "                                  m -- Generate 64-bit MacOS 'as -arch arm64' compatible assembler code to filename.s\n" );
    printf( "                                  r -- Generate 64-bit RISC-V 64-bit GNU 'as' compatible assembler code to filename.s\n" );
    printf( "                                  x -- Generate 64-bit Windows x64 'ml64' compatible assembler code to filename.asm\n" );
    printf( "                 -d               Generate a dollar sign $ at the end of execution for Apple 1 apps\n" );
#endif
    printf( "                 -e               Show execution count and time for each line\n" );
    printf( "                 -l               Show 'pcode' listing\n" );
#ifdef BA_ENABLE_COMPILER
    printf( "                 -o               Don't do expression optimization for assembly code\n" );
#endif
    printf( "                 -p               Show parse time for input file\n" );
    printf( "                 -q               Quiet. Don't show start and end messages in interpreter or compiled code\n" );
#ifdef BA_ENABLE_COMPILER
    printf( "                 -r               Don't use registers for variables in assembly code\n" );
#endif
    printf( "                 -t               Show debug tracing\n" );
    printf( "                 -x               Parse only; don't execute the code\n" );
#ifdef BA_ENABLE_COMPILER
    printf( "  notes:  --  Assembly instructions are located at the top of generated files\n" );
#endif
    exit( 1 );
} //Usage

int main( int argc, char * argv[] )
{
    Usage();
    return 0;
}