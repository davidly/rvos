# rvos
Loads and runs Linux RISC-V .elf files on Linux, MacOS, and Windows.

    usage: rvos <elf_executable>

    arguments:    -e     just show information about the elf executable; don't actually run it
                  -g     (internal) generate rcvtable.txt
                  -h:X   # of meg for the heap (brk space) 0..1024 are valid. default is 10
                  -i     if -t is set, also enables risc-v instruction tracing
                  -m:X   # of meg for mmap space 0..1024 are valid. default is 0
                  -p     shows performance information at app exit
                  -t     enable debug tracing to rvos.log

* Notes:
    * This is a simplistic 64-bit RISC-V M Mode emulator; it's an AEE (Application Execution Environment) that exposes a Linux-like ABI.
    * Only physical memory is supported.
    * Compressed rvc 16-bit instructions are supported, though apps run about 5% slower.
    * Single and double precision floating point instructions are implemented.
    * Atomic and fence instructions are implemented assuming there is just one core.
    * Much of the Gnu C Runtime is tested -- memory, fopen/open families of IO, date/time, printf/sprintf, math, etc.
    * No network system calls are implemented.
    * I also tested with the BASIC test suite for my compiler BA, which targets RISC-V.
    * The emulator is about as fast as the 400Mhz K210 physical processor when run on an AMD 5950x.
    * The tests folder has test apps written in C. These build with both old and recent g++ for RISC-V.
    * rvos has been built and tested on Windows (x86, amd64 and arm64), MacOS (arm64), and Linux (RISC-V, amd64, arm32, and arm64).
    * The emulator can run itself nested arbitrarily deeply. Perf is about 64x slower for each nesting.
    * Linux system call emulation isn't great. It's just good enough to test RISC-V emulation and run apps built with g++ and Rust.
    * Build for MacOS using the Linux build scripts (m.sh, etc.)
    * rvos only runs static-linked RISC-V .elf files. Use the -static flag with ld or the g++ command-line.
    * Gnu CC torture execution tests were run on the emulator.
    * The environment variable "OS=RVOS" is set for apps running in the emulator.
    * Tested with the 6502 / Apple 1 emulator in my ntvao repo built for RISC-V.
    * Tested with the Z80 / CP/M 2.2 emulator in my ntvcm repo built for RISC-V.
    * Tested with the 8086 / DOS emulator in my ntvdm repo built for RISC-V (for character-mode apps).
    * Does not work with apps generated by Go, whose runtime apparently requires mmap to hand back requested addresses.
    * mmap calls work provided -m is set, requests are backed by the pagefile, and memory is committed (not reserved).
    * Works with Rust apps provided they're statically linked, e.g.: rustc -O -C target-feature=+crt-static sample.rs
    

* Files:
    * riscv.?xx       Emulates a RISC-V processor in M mode.
    * rvos.cxx        Loads .elf files and executes them using the riscv.?xx emulator. Emulates some Linux syscalls.
    * rvos.h          Header file for rvos and for apps that call into rvos syscalls
    * rvctable.txt    RVC compressed to 32-bit RISC-V instruction lookup table. Generated with rvos -g
    * m.bat           Builds rvos on Windows. mr.bat is the same but release-optimized
    * m.sh            Builds rvos on Linux. mr.sh is the same but is release-optimized.
    * mg.bat          Builds rvos on Windows using Mingw64 g++. mgr.bat for release-optimized.
    * mt.sh           Builds test .c apps on Linux
    * mrvoself.sh     Builds rvos.elf so it can be run in an rvos emulator on Linux
    * marm64.bat      Builds rvos on Arm64 Windows
    * mrvos.bat       Builds rvos.elf targeting a RISC-V Linux machine, which rvos can execute.
    * rt.bat          Runs tests in the test folder, assuming they're built
    * rt.sh           Runs tests on Linux
    * baseline_test_rvos.txt  Expected output of rt.bat, rt.sh, rti.bat, rti.sh, etc.
    * kendryte.ld     Gnu ld configuration file to target SiPeed K210 RISC-V hardware and RVOS
    * djltrace.hxx    debug tracing class
    * djl_os.hxx      eases porting among various operating systems
    * djl_con.hxx     os-dependent console and display functions
    * djl_128.hxx     128-bit integer support
    * djl_mmap.hxx    very simplistic mmap implementation
    * words.txt       Used by tests\an.c test app to generate anagrams 

The tests folder has a number of small C/C++ programs that can be compiled using mariscv.bat on Windows or
mt.sh on Linux. The .elf files produced can be run by rvos. If the app will use more 10 meg of RAM for the 
heap, use rvos' -h argument to specify how much RAM to allocate. The anagram generater an can use up to 
30MB in some cases.

* Test files:
    * an.c        Anagram generator
    * ba.c        Simple basic interpreter and compiler (can target RISC-V, 6502, 8080, 8086, x86, amd64, arm32, arm64)
    * e.c         Computes e
    * glob.c      Global C++ class to test construction/destruction before/after main().
    * mysort.c    Sorts a text input file and produces a text output file
    * sieve.c     Finds prime numbers
    * t.c         Tests integers of various sizes: 8, 16, 32, 64, 128
    * tap.c       Computes Apéry's number
    * tcrash.c    Tests out of bounds memory, pc, and sp references
    * td.c        Tests doubles
    * tdir.c      Tests directory functions (only works with the newer g++ tools)
    * tenv.c      Tests using C environment functions
    * terrno.c    Tests errno functions
    * tf.c        Tests floating point numbers
    * tfile.c     Tests file I/O
    * tins.s      Test various instructions I couldn't get g++ or Rust apps to use
    * tm.c        Lightly test malloc/calloc/free
    * tphi.c      Computes phi
    * tpi.c       Computes PI
    * ts.c        Tests bit shifts on various integer types
    * ttime.c     Tests retrieving and showing the current time
    * ttt.c       Proves you can't win at tic-tac-toe

I tested the apps above with 3 versions of g++ that target RISC-V. Generated apps from each utilize different RISC-V instructions and Linux syscalls.

    * Installed on Windows AMD64 on February 19, 2023 with Arduino with the SiPeed K210: riscv64-unknown-elf-g++.exe (GCC) 8.2.0
    * I cloned and built https://github.com/riscv-collab/riscv-gnu-toolchain on March 9, 2023 using Ubuntu AMD64: riscv64-unknown-linux-gnu-c++ (g2ee5e430018) 12.2.0
    * Installed on Debian RISC-V September 3, 2023 using sudo apt-get install gpp: g++ (Debian 13.1.0-6) 13.1.0
    
To run the rvos emulator nested in the rvos emulator, on Linux/MacOS/Windows issue a command like:

    rvos -h:60 rvos.elf -h:40 an phoebe bridgers
    
That gives the inner emulator 60 megs of RAM so it can give 40 megs to the AN anagram generator (an.c in the 
tests folder) so it can find the 485 3-word anagrams for that text including bog bride herpes. Running the emulator
in the emulator makes it aout 64x slower.

The Gnu g++ compiler produces code that's about 10% faster than the Microsoft C++ compiler.

ttt_riscv.s is a sample assembler app. This requires rvos_shell.s, which has _start and a function to print text to the console
that apps can call. I used the SiPeed Maixduino Arduino Gnu tools for the K210 RISC-V machine. The shell is slightly
different on the actual hardware -- it calls bamain (not main) and the text print function prints to the device's LCD.

* Sample assembler files:
    * make_s.bat:     test app build script. compiles and links ttt_riscv.s and riscv_shell.s to make ttt_riscv.elf
    * ttt_riscv.s:    app to prove you can't win a nuclear war per War Games
    * rvos_shell.s:   wrapper for the app (defines _start and rvos ABI)
    * minimal.ino:    Arduino IDE equivalent C++ app for riscv_shell.s on the K210 hardware

If you get a runtime error like this then use the -h flag to reserve more RAM for the heap.

        terminate called after throwing an instance of 'std::bad_alloc'
          what():  std::bad_alloc
          
RVOS has a few simple checks for memory references outside of bounds with debug builds. When detected it shows 
some state and exits. Real RISC-V memory protection instructions are not implemented. For example:

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

Tracing with the -t and -i flags shows execution information including function names (if the RISC-V elf
image was built with symbols using -ggdb) and registers. For example, tcrash.elf is an app that makes 
an illegal access to memory. It was used for the crash dump above. Here is a simple crashing app tbad.c:

    int main()
    {
        char * pbad = (char *) 0x200;
        *pbad = 10;
    } //main

Here is the trace listing for tbad.c using rvos -t -i tbad:

    old and new console output mode: 0003, 000f
    loading image tbad.elf
    header fields:
      entry address: 100d8
      program entries: 3
      program header entry size: 56
      program offset: 64 == 40
      section entries: 18
      section header entry size: 64
      section offset: 6656 == 1a00
      flags: 3
    program header 0 at offset 64
      type: 1 / load
      offset in image: 1000
      virtual address: 10000
      physical address: 10000
      file size: 1f0
      memory size: 1f0
      alignment: 1000
    program header 1 at offset 120
      type: 1 / load
      offset in image: 11f0
      virtual address: 101f0
      physical address: 101f0
      file size: 0
      memory size: 10
      alignment: 1000
    program header 2 at offset 176
      type: 0 / unused
      offset in image: 0
      virtual address: 10200
      physical address: 10200
      file size: 0
      memory size: 38
      alignment: 8
    memory_size of content to load from elf file: 238
    section header 0 at offset 6656 == 1a00
      type: 0 / unused
      flags: 0 / 
      address: 0
      offset: 0
      size: 0
    section header 1 at offset 6720 == 1a40
      type: 1 / program data
      flags: 6 / alloc, executable, 
      address: 10000
      offset: 1000
      size: 1e0
    section header 2 at offset 6784 == 1a80
      type: e / initialization functions
      flags: 3 / write, alloc, 
      address: 101e0
      offset: 11e0
      size: 8
    section header 3 at offset 6848 == 1ac0
      type: f / termination functions
      flags: 3 / write, alloc, 
      address: 101e8
      offset: 11e8
      size: 8
    section header 4 at offset 6912 == 1b00
      type: 1 / program data
      flags: 1 / write, 
      address: 101f0
      offset: 11f0
      size: 0
    section header 5 at offset 6976 == 1b40
      type: 1 / program data
      flags: 1 / write, 
      address: 101f0
      offset: 11f0
      size: 0
    section header 6 at offset 7040 == 1b80
      type: 8 / nobits
      flags: 3 / write, alloc, 
      address: 101f0
      offset: 11f0
      size: 10
    section header 7 at offset 7104 == 1bc0
      type: 8 / nobits
      flags: 3 / write, alloc, 
      address: 10200
      offset: 0
      size: 38
    section header 8 at offset 7168 == 1c00
      type: 1 / program data
      flags: 30 / merge, asciz strings, 
      address: 0
      offset: 11f0
      size: 11
    section header 9 at offset 7232 == 1c40
      type: 1 / program data
      flags: 0 / 
      address: 0
      offset: 1201
      size: 6f
    section header 10 at offset 7296 == 1c80
      type: 1 / program data
      flags: 0 / 
      address: 0
      offset: 1270
      size: 60
    section header 11 at offset 7360 == 1cc0
      type: 1 / program data
      flags: 0 / 
      address: 0
      offset: 12d0
      size: 30
    section header 12 at offset 7424 == 1d00
      type: 1 / program data
      flags: 0 / 
      address: 0
      offset: 1300
      size: 20
    section header 13 at offset 7488 == 1d40
      type: 1 / program data
      flags: 0 / 
      address: 0
      offset: 1320
      size: 5a
    section header 14 at offset 7552 == 1d80
      type: 1 / program data
      flags: 30 / merge, asciz strings, 
      address: 0
      offset: 137a
      size: eb
    section header 15 at offset 7616 == 1dc0
      type: 2 / symbol table
      flags: 0 / 
      address: 0
      offset: 1468
      size: 390
    section header 16 at offset 7680 == 1e00
      type: 3 / string table
      flags: 0 / 
      address: 0
      offset: 17f8
      size: 15a
    section header 17 at offset 7744 == 1e40
      type: 3 / string table
      flags: 0 / 
      address: 0
      offset: 1952
      size: ab
    elf image has 19 usable symbols:
                 address              size  name
                   10000                10  main
                   10010                24  deregister_tm_clones
                   10034                2c  register_tm_clones
                   10060                42  __do_global_dtors_aux
                   100a2                36  frame_dummy
                   100d8                24  _start
                   100fc                20  rvos_print_text
                   1011c                20  rvos_get_datetime
                   1013c                1c  rvos_rand
                   10158                20  rvos_exit
                   10178                20  rvos_print_double
                   10198                1c  rvos_gettimeofday
                   101b4                20  rvos_trace_instructions
                   101d4                 c  rvos_sp_add
                   101e0                 8  __frame_dummy_init_array_entry
                   101e8                18  __do_global_dtors_aux_fini_array_entry
                   10200                 0  __TMC_END__
                   10200                 8  completed.5485
                   10208                30  object.5490
      read type load: 1f0 bytes into physical address 10000 - 101ef then uninitialized to 101ef 
        0000  93 07 a0 00 23 00 f0 20 13 05 00 00 67 80 00 00 : 17 05 00 00 13 05 05 1f 97 07 00 00 93 87 87 1e  ....#.. ....g...................
        0020  63 89 a7 00 17 03 ff ff 13 03 c3 fd 63 03 03 00 : 02 83 82 80 17 05 00 00 13 05 c5 1c 97 05 00 00  c...........c...................
        0040  93 85 45 1c 89 8d 8d 85 89 47 b3 c5 f5 02 81 c9 : 17 03 ff ff 13 03 03 fb 63 03 03 00 02 83 82 80  ..E......G..............c.......
        0060  97 07 00 00 83 c7 07 1a 85 ef 41 11 06 e4 97 00 : 00 00 e7 80 20 fa 97 07 ff ff 93 87 a7 f8 89 cb  ..........A......... ...........
      argument 0 is 'tbad.elf', at vm address 10380
    ptz_data: 'TZ=PacificStandardTime+8'
    args_len 9, penv_data 000002D758F5D3EA
        0000  80 03 01 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        0020  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        0040  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        0060  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        0080  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        00a0  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        00c0  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        00e0  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        0100  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        0120  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        0140  74 62 61 64 2e 65 6c 66 00 00 4f 53 3d 52 56 4f : 53 00 54 5a 3d 50 61 63 69 66 69 63 53 74 61 6e  tbad.elf..OS=RVOS.TZ=PacificStan
        0160  64 61 72 64 54 69 6d 65 2b 38 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  dardTime+8......................
        0180  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        01a0  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        01c0  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        01e0  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        0200  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        0220  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        0240  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        0260  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        0280  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        02a0  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        02c0  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        02e0  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        0300  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        0320  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        0340  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        0360  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        0380  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        03a0  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        03c0  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        03e0  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
        0400  00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
    the OS environment argument is at VM address 1038a
    the TZ environment argument is at VM address 10392
    stack at start (beginning with argc) -- 112 bytes at address 000002D75997D630:
      0000  01 00 00 00 00 00 00 00 80 03 01 00 00 00 00 00 : 00 00 00 00 00 00 00 00 92 03 01 00 00 00 00 00  ................................
      0020  8a 03 01 00 00 00 00 00 00 00 00 00 00 00 00 00 : 19 00 00 00 00 00 00 00 f0 0f a3 00 00 00 00 00  ................................
      0040  06 00 00 00 00 00 00 00 00 10 00 00 00 00 00 00 : 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00  ................................
      0060  bb c1 78 6d 7c 3b c9 02 d6 2c b6 5e b8 92 f0 ec :                                                  ..xm|;...,.^....
    memory map from highest to lowest addresses:
      first byte beyond allocated memory:                 a31000
      <mmap arena>                                        (0 = 0 bytes)
      mmap start adddress:                                a31000
      <align to 4k-page for mmap allocations>
      start of aux data:                                  a30640
      <random, alignment, aux recs, env, argv>            (112 == 70 bytes)
      initial stack pointer g_top_of_stack:               a305d0
      <stack>                                             (130960 == 1ff90 bytes)
      last byte stack can use (g_bottom_of_stack):        a10640
      <unallocated space between brk and the stack>       (10485760 == a00000 bytes)
      end_of_data / current brk:                          10640
      <argv data, pointed to by argv array above>         (1024 == 400 bytes)
      start of argv data:                                 10240
      <uninitialized data per the .elf file>              (80 == 50 bytes)
      first byte of uninitialized data:                   101f0
      <initialized data from the .elf file>
      <code from the .elf file>
      initial pc execution_addess:                        100d8
      <code per the .elf file>
      start of the address space per the .elf file:       10000
    vm memory first byte beyond:     000002D75997E060
    vm memory start:                 000002D758F5D060
    memory_size:                     0xa21000 == 10620928
    risc-v compressed instructions:  yes
    pc    100d8 _start
                 op    13503 a0 0 a1 0 a2 0 a3 0 a4 0 a5 0 gp 0 t0 0 t1 0 s0 0 s1 0, s7 0 ra 0 sp a305d0 t  0 I => ld      a0, 0(sp)  # 0(a305d0)
    pc    100dc  op   810593 a0 1 a1 0 a2 0 a3 0 a4 0 a5 0 gp 0 t0 0 t1 0 s0 0 s1 0, s7 0 ra 0 sp a305d0 t  4 I => addi    a1, sp, 8
    pc    100e0  op   150413 a0 1 a1 a305d8 a2 0 a3 0 a4 0 a5 0 gp 0 t0 0 t1 0 s0 0 s1 0, s7 0 ra 0 sp a305d0 t  4 I => addi    s0, a0, 1
    pc    100e4  op   800493 a0 1 a1 a305d8 a2 0 a3 0 a4 0 a5 0 gp 0 t0 0 t1 0 s0 2 s1 0, s7 0 ra 0 sp a305d0 t  4 I => addi    s1, zero, 8
    pc    100e8  op  2940433 a0 1 a1 a305d8 a2 0 a3 0 a4 0 a5 0 gp 0 t0 0 t1 0 s0 2 s1 8, s7 0 ra 0 sp a305d0 t  c R => mul     s0, s0, s1 # 2 * 8
    pc    100ec  op   858633 a0 1 a1 a305d8 a2 0 a3 0 a4 0 a5 0 gp 0 t0 0 t1 0 s0 10 s1 8, s7 0 ra 0 sp a305d0 t  c R => add     a2, a1, s0 # a305d8 + 10
    pc    100f0  op f11ff0ef a0 1 a1 a305d8 a2 a305e8 a3 0 a4 0 a5 0 gp 0 t0 0 t1 0 s0 10 s1 8, s7 0 ra 0 sp a305d0 t 1b J => jal     -240 #    10000
    pc    10000 main
                 op   a00793 a0 1 a1 a305d8 a2 a305e8 a3 0 a4 0 a5 0 gp 0 t0 0 t1 0 s0 10 s1 8, s7 0 ra 100f4 sp a305d0 t  4 I => addi    a5, zero, 10
    pc    10004  op 20f00023 a0 1 a1 a305d8 a2 a305e8 a3 0 a4 0 a5 a gp 0 t0 0 t1 0 s0 10 s1 8, s7 0 ra 100f4 sp a305d0 t  8 S => sb      a5, 512(zero)  #   a, 512(0)
    rvos (amd64) fatal error: memory reference prior to address space: 200
    pc: 10004 main + 4
    address space 10000 to a31000
      zero:                0,   ra:            100f4,   sp:           a305d0,   gp:                0, 
        tp:                0,   t0:                0,   t1:                0,   t2:                0, 
        s0:               10,   s1:                8,   a0:                1,   a1:           a305d8, 
        a2:           a305e8,   a3:                0,   a4:                0,   a5:                a, 
        a6:                0,   a7:                0,   s2:                0,   s3:                0, 
        s4:                0,   s5:                0,   s6:                0,   s7:                0, 
        s8:                0,   s9:                0,  s10:                0,  s11:                0, 
        t3:                0,   t4:                0,   t5:                0,   t6:                0, 
    Built for amd64 debug on 29 Dec 23 13:04:12 by msft C++ ver 1939 on windows


