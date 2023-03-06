# rvos
RISC-V emulator and minimal "OS". Loads and runs .elf files on Windows.

    usage: rvos <elf_executable>

   arguments:    -e     just show information about the elf executable; don't actually run it
                 -g     (internal) generate rcvtable.txt
                 -h:X   # of meg for the heap (brk space) 0..1024 are valid. default is 1
                 -i     if -t is set, also enables risc-v instruction tracing
                 -p     shows performance information at app exit
                 -t     enable debug tracing to rvos.log

Notes:

    This is a simplistic 64-bit RISC-V emulator.
    Only physical memory is supported.
    Only a subset of instructions are implemented (enough to to run my test apps).
    Compressed rvc 16-bit instructions are supported, though apps run about 5% slower.
    Float instructions are implemented; double are not.
    I tested with a variety of C and C++ apps compiled with g++. 
    Some of the Gnu C Runtime is tested -- memory, fopen/open families of IO, printf/sprintf.
    I also tested with the BASIC test suite for my compiler BA, which targets risc-v.
    It's slightly faster than the 400Mhz K210 physical processor when emulated on my AMD 5950x machine.
    The tests folder has test apps written in C.

ttt_riscv.s is a sample app. This requires rvos_shell.s, which has _start and a function to print text to the console
that apps can call. I used the SiPeed Maixduino Arduino Gnu tools for the K210 RISC-V machine. The shell is slightly
different on the actual hardware -- it calls bamain (not main) and the text print function prints to the device's LCD.

    make_s.bat:     test app build script. compiles and links ttt_riscv.s and riscv_shell.s to make ttt_riscv.elf
    ttt_riscv.s:    app to prove you can't win a nuclear war per War Games
    rvos_shell.s:   wrapper for the app (defines _start and rvos ABI)
    minimal.ino:    Arduino IDE equivalent C++ app for riscv_shell.s on the K210 hardware
    
The tests folder has a number of small C/C++ programs that can be compiled using mariscv.bat. The .elf files produced
can be run by rvos. If the app will use more 1 meg of RAM for the heap, uses rvos' /h argument to specify how much
RAM to allocate. The anagram generater an can use up to 30MB in some cases.

RVOS has a few simple checks for memory references outside of bounds. When detected it shows some stat and exits.
For example:

    rvos fatal error: memory reference prior to address space: 200
    pc: 80000002 main
    address space 80000000 to 801105c0
      zero:                0,   ra:         800000d6,   sp:         801104c0,   gp:                0,
        tp:                0,   t0:                0,   t1:                0,   t2:                0,
        s0:                0,   s1:                0,   a0:                1,   a1:         800001c0,
        a2:                0,   a3:                0,   a4:                0,   a5:                a,
        a6:                0,   a7:                0,   s2:                0,   s3:                0,
        s4:                0,   s5:                0,   s6:                0,   s7:                0,
        s8:                0,   s9:                0,  s10:                0,  s11:                0,
        t3:                0,   t4:                0,   t5:                0,   t6:                0,


