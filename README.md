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
    I tested with a variety of C apps compiled with g++. 
    Some of the Gnu C Runtime is tested -- memory, fopen/open families of IO, printf/sprintf.
    I also tested with the BASIC test suite for my compiler BA, which targets risc-v.
    It's slightly faster than the 400Mhz K210 physical processor when emulated on my AMD 5950x machine.
    The tests folder has test apps written in C.

A sample app is ttt_riscv.s. Also build riscv_shell.s, which has _start and a function to print text to the console
that apps can call. I used the SiPeed Maixduino Arduino Gnu tools for the K210 RISC-V machine. The shell is slightly
different on the actual hardware -- it calls bamain (not main) and the text print function prints to the device's LCD.

    mariscv.bat:    test app build script. compiles and links ttt_riscv.s and riscv_shell.s to make ttt_riscv.elf
    ttt_riscv.s:    app to prove you can't win a nuclear war per War Games
    riscv_shell.s:  wrapper for the app (calls main and has a print function)
    minimal.ino:    Arduino IDE equivalent C++ app for riscv_shell.s on the K210 hardware
    
