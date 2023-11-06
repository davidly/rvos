# rvos
Loads and runs Linux RISC-V .elf files on Linux, MacOS, and Windows.

    usage: rvos <elf_executable>

    arguments:    -e     just show information about the elf executable; don't actually run it
                  -g     (internal) generate rcvtable.txt
                  -h:X   # of meg for the heap (brk space) 0..1024 are valid. default is 10
                  -i     if -t is set, also enables risc-v instruction tracing
                  -p     shows performance information at app exit
                  -t     enable debug tracing to rvos.log

* Notes:
    * This is a simplistic 64-bit RISC-V M Mode emulator; it's an AEE (Application Execution Environment) that exposes a Linux-like ABI.
    * Only physical memory is supported.
    * Compressed rvc 16-bit instructions are supported, though apps run about 5% slower.
    * Single and double precision floating point instructions are implemented.
    * Atomic and fence instructions are implemented assuming there is just one core.
    * Much of the Gnu C Runtime is tested -- memory, fopen/open families of IO, date/time, printf/sprintf, math, etc.
    * I also tested with the BASIC test suite for my compiler BA, which targets RISC-V.
    * The emulator is about as fast as the 400Mhz K210 physical processor when run on an AMD 5950x.
    * The tests folder has test apps written in C. These build with both old and recent g++ for RISC-V.
    * rvos has been tested on Windows (amd64 and arm64), MacOS (arm64), and Linux (RISC-V, amd64, and arm64).
    * The emulator can run itself nested arbitrarily deeply. Perf is about 64x slower for each nesting.
    * Linux system call emulation isn't great. It's just good enough to test RISC-V emulation and run apps built with g++.
    * Build for MacOS using the Linux build scripts (m.sh, etc.)
    * rvos only runs static-linked RISC-V .elf files. Use the -static flag with ld or the g++ command-line.
    * Gnu CC torture execution tests were run on the emulator.
    * Built and tested on 32-bit ARM and 32-bit x86.
    * The environment variable "OS=RVOS" is set for apps running in the emulator.
    * Tested with the 6502 / Apple 1 emulator in my ntvao repo built for RISC-V.
    * Tested with the Z80 / CP/M 2.2 emulator in my ntvcm repo built for RISC-V.
    * Tested with the 8086 / DOS emulator in my ntvdm repo built for RISC-V (for character-mode apps).

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
    * words.txt       Used by tests\an.c test app to generate anagrams 

The tests folder has a number of small C/C++ programs that can be compiled using mariscv.bat on Windows or
mt.sh on Linux. The .elf files produced can be run by rvos. If the app will use more 10 meg of RAM for the 
heap, use rvos' /h argument to specify how much RAM to allocate. The anagram generater an can use up to 
30MB in some cases.

* Test files:
    * an.c        Anagram generator
    * ba.c        Simple basic interpreter and compiler (can target RISC-V, 6502, 8080, 8086, x86, amd64, arm32, arm64)
    * e.c         Computes e
    * mysort.c    Sorts a text input file and produces a text output file
    * sieve.c     Finds prime numbers
    * t.c         Tests integers of various sizes: 8, 16, 32, 64, 128
    * tap.c       Computes Apéry's number
    * tcrash.c    Tests out of bounds memory, pc, and sp references
    * td.c        Tests doubles
    * tdir.c      Tests directory function (only works with the newer g++ tools)
    * tenv.c      Tests using C environment functions
    * tf.c        Tests floating point numbers
    * tfile.c     Tests file I/O
    * tins.s      Test various instructions I couldn't get g++ to produce
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

    rvos /h:60 rvos.elf /h:40 an phoebe bridgers
    
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

If you get a runtime error like this then use the /h flag to reserve more RAM for the heap.

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

Here is the trace listing for tbad.c:

    loading image tests\tbad.elf
    header fields:
      entry address: 100d8
      program entries: 3
      program header entry size: 56
      program offset: 64 == 40
      section entries: 18
      section header entry size: 64
      section offset: 6496 == 1960
      flags: 3
    program header 0 at offset 64
      type: 1 / load
      offset in image: 1000
      virtual address: 10000
      physical address: 10000
      file size: 1c0
      memory size: 1c0
      alignment: 1000
    program header 1 at offset 120
      type: 1 / load
      offset in image: 11c0
      virtual address: 0
      physical address: 0
      file size: 0
      memory size: 0
      alignment: 1000
    program header 2 at offset 176
      type: 0 / unused
      offset in image: 0
      virtual address: 101c0
      physical address: 101c0
      file size: 0
      memory size: 38
      alignment: 8
    section header 0 at offset 6496 == 1960
      type: 0 / unused
      flags: 0 / 
      address: 0
      offset: 0
      size: 0
    section header 1 at offset 6560 == 19a0
      type: 1 / program data
      flags: 6 / alloc, executable, 
      address: 10000
      offset: 1000
      size: 1b0
    section header 2 at offset 6624 == 19e0
      type: e / unknown
      flags: 3 / write, alloc, 
      address: 101b0
      offset: 11b0
      size: 8
    section header 3 at offset 6688 == 1a20
      type: f / unknown
      flags: 3 / write, alloc, 
      address: 101b8
      offset: 11b8
      size: 8
    section header 4 at offset 6752 == 1a60
      type: 1 / program data
      flags: 1 / write, 
      address: 101c0
      offset: 11c0
      size: 0
    section header 5 at offset 6816 == 1aa0
      type: 1 / program data
      flags: 1 / write, 
      address: 101c0
      offset: 11c0
      size: 0
    section header 6 at offset 6880 == 1ae0
      type: 1 / program data
      flags: 3 / write, alloc, 
      address: 101c0
      offset: 11c0
      size: 0
    section header 7 at offset 6944 == 1b20
      type: 8 / nobits
      flags: 3 / write, alloc, 
      address: 101c0
      offset: 0
      size: 38
    section header 8 at offset 7008 == 1b60
      type: 1 / program data
      flags: 30 / merge, asciz strings, 
      address: 0
      offset: 11c0
      size: 11
    section header 9 at offset 7072 == 1ba0
      type: 1 / program data
      flags: 0 / 
      address: 0
      offset: 11d1
      size: 6f
    section header 10 at offset 7136 == 1be0
      type: 1 / program data
      flags: 0 / 
      address: 0
      offset: 1240
      size: 60
    section header 11 at offset 7200 == 1c20
      type: 1 / program data
      flags: 0 / 
      address: 0
      offset: 12a0
      size: 30
    section header 12 at offset 7264 == 1c60
      type: 1 / program data
      flags: 0 / 
      address: 0
      offset: 12d0
      size: 20
    section header 13 at offset 7328 == 1ca0
      type: 1 / program data
      flags: 0 / 
      address: 0
      offset: 12f0
      size: 5a
    section header 14 at offset 7392 == 1ce0
      type: 1 / program data
      flags: 30 / merge, asciz strings, 
      address: 0
      offset: 134a
      size: eb
    section header 15 at offset 7456 == 1d20
      type: 2 / symbol table
      flags: 0 / 
      address: 0
      offset: 1438
      size: 348
    section header 16 at offset 7520 == 1d60
      type: 3 / string table
      flags: 0 / 
      address: 0
      offset: 1780
      size: 12f
    section header 17 at offset 7584 == 1da0
      type: 3 / string table
      flags: 0 / 
      address: 0
      offset: 18af
      size: ab
    elf image has 18 usable symbols:
                 address              size  name
                   10000                10  main
                   10010                24  deregister_tm_clones
                   10034                2c  register_tm_clones
                   10060                42  __do_global_dtors_aux
                   100a2                36  frame_dummy
                   100d8                14  _start
                   100ec                20  rvos_print_text
                   1010c                1c  rvos_rand
                   10128                20  rvos_exit
                   10148                20  rvos_print_double
                   10168                1c  rvos_gettimeofday
                   10184                20  rvos_trace_instructions
                   101a4                 c  rvos_sp_add
                   101b0                 8  __frame_dummy_init_array_entry
                   101b8                 8  __do_global_dtors_aux_fini_array_entry
                   101c0                 0  completed.5485
                   101c0                 8  __TMC_END__
                   101c8                30  object.5490
      argument 0 is 'tests\tbad.elf', at vm address 10340
    vm memory start:                 0000019E13A65060
    g_perrno:                        0000000000000000
    risc-v compressed instructions:  1
    vm g_base_address                10000
    memory_size:                     120600
    g_brk_commit:                    100000
    g_stack_commit:                  20000
    arg_data:                        200
    g_brk_address:                   600
    g_end_of_data:                   600
    g_bottom_of_stack:               100600
    g_top_of_stack:                  1305b0
    space used by startup data:      50
    execution_addess                 100d8
    pc    100d8 _start op    13503 a0 0 a1 0 a2 0 a3 0 a4 0 a5 0 s0 0 s1 0 ra 0 sp 1305b0 t  0 I => ld      a0, 0(sp)  # 0(1305b0)
    pc    100dc  op   810593 a0 1 a1 0 a2 0 a3 0 a4 0 a5 0 s0 0 s1 0 ra 0 sp 1305b0 t  4 I => addi    a1, sp, 8
    pc    100e0  op f21ff0ef a0 1 a1 1305b8 a2 0 a3 0 a4 0 a5 0 s0 0 s1 0 ra 0 sp 1305b0 t 1b J => jal     -224 #    10000
    pc    10000 main op   a00793 a0 1 a1 1305b8 a2 0 a3 0 a4 0 a5 0 s0 0 s1 0 ra 100e4 sp 1305b0 t  4 I => addi    a5, zero, 10
    pc    10004  op 20f00023 a0 1 a1 1305b8 a2 0 a3 0 a4 0 a5 a s0 0 s1 0 ra 100e4 sp 1305b0 t  8 S => sb      a5, 512(zero)  #   a, 512(0)
    rvos fatal error: memory reference prior to address space: 200
    pc: 10004 main
    address space 10000 to 130600
      zero:                0,   ra:            100e4,   sp:           1305b0,   gp:                0, 
        tp:                0,   t0:                0,   t1:                0,   t2:                0, 
        s0:                0,   s1:                0,   a0:                1,   a1:           1305b8, 
        a2:                0,   a3:                0,   a4:                0,   a5:                a, 
        a6:                0,   a7:                0,   s2:                0,   s3:                0, 
        s4:                0,   s5:                0,   s6:                0,   s7:                0, 
        s8:                0,   s9:                0,  s10:                0,  s11:                0, 
        t3:                0,   t4:                0,   t5:                0,   t6:                0, 
      
