# rvos
Loads and runs Linux RISC-V .elf files on Linux and Windows.

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
    rvos can run rvos on both Windows and Linux; the emulator is good enough to emulate itself.

ttt_riscv.s is a sample app. This requires rvos_shell.s, which has _start and a function to print text to the console
that apps can call. I used the SiPeed Maixduino Arduino Gnu tools for the K210 RISC-V machine. The shell is slightly
different on the actual hardware -- it calls bamain (not main) and the text print function prints to the device's LCD.

    make_s.bat:     test app build script. compiles and links ttt_riscv.s and riscv_shell.s to make ttt_riscv.elf
    ttt_riscv.s:    app to prove you can't win a nuclear war per War Games
    rvos_shell.s:   wrapper for the app (defines _start and rvos ABI)
    minimal.ino:    Arduino IDE equivalent C++ app for riscv_shell.s on the K210 hardware
    
Other files:

    riscv.?xx       Emulates a RISC-V processor
    rvos.cxx        Loads .elf files and executes them using the riscv.?xx emulator. Emulates some Linux syscalls.
    rvos.h          Header file for rvos and apps that call into rvos syscalls not shared with Linux
    rvctable.txt    RVC compressed to 16-bit RISC-V instruction lookup table. Generated by rvos -g
    m.bat           Builds rvos on Windows
    m.sh            Builds rvos on Linux
    marm64.bat      Builds rvos on Arm64 Windows
    mrvos.bat       Builds rvos.elf targeting a RISC-V Linux machine, which rvos can execute.
    rt.bat          Runs Tests in the test folder, assuming they're built
    baseline_test_rvos.txt  Expected output of rt.bat
    kendryte.ld     Gnu ld configuration file to target SiPeed K210 RISC-V hardware and RVOS
    djltrace.hxx    debug tracing class
    words.txt       Used by tests\an.c test app to generate anagrams 

The tests folder has a number of small C/C++ programs that can be compiled using mariscv.bat. The .elf files produced
can be run by rvos. If the app will use more 1 meg of RAM for the heap, use rvos' /h argument to specify how much
RAM to allocate. The anagram generater an can use up to 30MB in some cases.

To run rvos in the rvos emulator, on Linux or Windows issue a command like:

    rvos /h:60 rvos.elf /h:40 an phoebe bridgers
    
That gives the inner emulator 60 megs of RAM so it can give 40 megs to the AN anagram generator (an.c in the 
tests folder) so it can find the 485 3-word anagrams for that text including bog bride herpes.

RVOS has a few simple checks for memory references outside of bounds. When detected it shows some state and exits.
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

Tracing with the -t and -i flags shows execution information including function names and registers. 
For example, tcrash.elf is an app that makes an illegal access to memory. It was used for the crash 
dump above. 

    extern "C" int main()
    {
        char * pbad = (char *) 0x200;
        *pbad = 10;
    } //main

Here is the trace listing for tcrash.elf:

    header fields:
      entry address: 800000d0
      program entries: 3
      program header entry size: 56
      program offset: 64 == 40
      section entries: 18
      section header entry size: 64
      section offset: 6384 == 18f0
      flags: 3
    program header 0 at offset 64
      type: 1 / load
      offset in image: 1000
      virtual address: 80000000
      physical address: 80000000
      file size: 178
      memory size: 178
      alignment: 1000
    program header 1 at offset 120
      type: 1 / load
      offset in image: 1178
      virtual address: 80000178
      physical address: 80000178
      file size: 0
      memory size: 8
      alignment: 1000
    program header 2 at offset 176
      type: 0 / unused
      offset in image: 0
      virtual address: 80000180
      physical address: 80000180
      file size: 0
      memory size: 38
      alignment: 8
    section header 0 at offset 6384 == 18f0
      type: 0 / unused
      flags: 0 / 
      address: 0
      offset: 0
      size: 0
    section header 1 at offset 6448 == 1930
      type: 1 / program data
      flags: 6 / alloc, executable, 
      address: 80000000
      offset: 1000
      size: 168
    section header 2 at offset 6512 == 1970
      type: 14 / unknown
      flags: 3 / write, alloc, 
      address: 80000168
      offset: 1168
      size: 8
    section header 3 at offset 6576 == 19b0
      type: 15 / unknown
      flags: 3 / write, alloc, 
      address: 80000170
      offset: 1170
      size: 8
    section header 4 at offset 6640 == 19f0
      type: 1 / program data
      flags: 1 / write, 
      address: 80000178
      offset: 1178
      size: 0
    section header 5 at offset 6704 == 1a30
      type: 1 / program data
      flags: 1 / write, 
      address: 80000178
      offset: 1178
      size: 0
    section header 6 at offset 6768 == 1a70
      type: 8 / unknown
      flags: 3 / write, alloc, 
      address: 80000178
      offset: 1178
      size: 8
    section header 7 at offset 6832 == 1ab0
      type: 8 / unknown
      flags: 3 / write, alloc, 
      address: 80000180
      offset: 0
      size: 38
    section header 8 at offset 6896 == 1af0
      type: 1 / program data
      flags: 30 / merge, asciz strings, 
      address: 0
      offset: 1178
      size: 11
    section header 9 at offset 6960 == 1b30
      type: 1 / program data
      flags: 0 / 
      address: 0
      offset: 1189
      size: 6f
    section header 10 at offset 7024 == 1b70
      type: 1 / program data
      flags: 0 / 
      address: 0
      offset: 11f8
      size: 60
    section header 11 at offset 7088 == 1bb0
      type: 1 / program data
      flags: 0 / 
      address: 0
      offset: 1258
      size: 30
    section header 12 at offset 7152 == 1bf0
      type: 1 / program data
      flags: 0 / 
      address: 0
      offset: 1288
      size: 20
    section header 13 at offset 7216 == 1c30
      type: 1 / program data
      flags: 0 / 
      address: 0
      offset: 12a8
      size: 5c
    section header 14 at offset 7280 == 1c70
      type: 1 / program data
      flags: 30 / merge, asciz strings, 
      address: 0
      offset: 1304
      size: eb
    section header 15 at offset 7344 == 1cb0
      type: 2 / symbol table
      flags: 0 / 
      address: 0
      offset: 13f0
      size: 330
    section header 16 at offset 7408 == 1cf0
      type: 3 / string table
      flags: 0 / 
      address: 0
      offset: 1720
      size: 125
    section header 17 at offset 7472 == 1d30
      type: 3 / string table
      flags: 0 / 
      address: 0
      offset: 1845
      size: ab
    elf image has 17 symbols
                 address              size  name
                80000000                 a  main
                8000000a                24  deregister_tm_clones
                8000002e                2c  register_tm_clones
                8000005a                42  __do_global_dtors_aux
                8000009c                34  frame_dummy
                800000d0                12  _start
                800000e2                16  rvos_print_text
                800000f8                10  rvos_rand
                80000108                16  rvos_exit
                8000011e                16  rvos_print_double
                80000134                12  rvos_gettimeofday
                80000146                22  rvos_trace_instructions
                80000168                 8  __frame_dummy_init_array_entry
                80000170                10  __do_global_dtors_aux_fini_array_entry
                80000180                 0  completed.5485
                80000180                 8  __TMC_END__
                80000188                30  object.5490
      argument 0 is 'tests\tcrash.elf', at vm address 80000260
    vm memory start:                 0000018A057F3060
    g_perrno:                        0000000000000000
    risc-v compressed instructions:  1
    vm g_base_address                80000000
    memory_size:                     1105c0
    g_brk_commit:                    100000
    g_stack_commit:                  10000
    g_arg_data:                      1c0
    g_brk_address:                   5c0
    g_end_of_data:                   5c0
    g_bottom_of_stack:               1005c0
    initial sp offset (memory_size): 1105c0
    execution_addess                 800000d0
    pc 800000d0 _start op f0010113 a0 1 a1 800001c0 a2 0 a3 0 a4 0 a5 0 s0 0 s1 0 ra 0 sp 801105c0 t  4 I => addi    sp, sp, -256
    pc 800000d2  op f2fff0ef a0 1 a1 800001c0 a2 0 a3 0 a4 0 a5 0 s0 0 s1 0 ra 0 sp 801104c0 t 1b J => jal     -210 # 80000000
    pc 80000000 main op   a00793 a0 1 a1 800001c0 a2 0 a3 0 a4 0 a5 0 s0 0 s1 0 ra 800000d6 sp 801104c0 t  4 I => addi    a5, zero, 10
    pc 80000002  op 20f00023 a0 1 a1 800001c0 a2 0 a3 0 a4 0 a5 a s0 0 s1 0 ra 800000d6 sp 801104c0 t  8 S => sb      a5, 512(zero)  #   a, 512(0)
    rvos fatal error: memory reference prior to address space: 200
    pc: 80000002 main
