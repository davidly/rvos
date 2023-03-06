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

Tracing with the -t and -i flags shows execution information including function names and registers. 
For example, tcrash.elf is an app that makes an illegal access to memory. It was used for the crash 
dump above. Here is the trace listing:

    header fields:
      entry address: 800000d0
      program entries: 3
      program header entry size: 56
      program offset: 64 == 40
      section entries: 18
      section header entry size: 64
      section offset: 12648 == 3168
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
    section header 0 at offset 12648 == 3168
      type: 0 / unused
      flags: 0 / 
      address: 0
      offset: 0
      size: 0
    section header 1 at offset 12712 == 31a8
      type: 1 / program data
      flags: 6 / alloc, executable, 
      address: 80000000
      offset: 1000
      size: 168
    section header 2 at offset 12776 == 31e8
      type: 14 / unknown
      flags: 3 / write, alloc, 
      address: 80000168
      offset: 1168
      size: 8
    section header 3 at offset 12840 == 3228
      type: 15 / unknown
      flags: 3 / write, alloc, 
      address: 80000170
      offset: 1170
      size: 8
    section header 4 at offset 12904 == 3268
      type: 1 / program data
      flags: 1 / write, 
      address: 80000178
      offset: 1178
      size: 0
    section header 5 at offset 12968 == 32a8
      type: 1 / program data
      flags: 1 / write, 
      address: 80000178
      offset: 1178
      size: 0
    section header 6 at offset 13032 == 32e8
      type: 8 / unknown
      flags: 3 / write, alloc, 
      address: 80000178
      offset: 1178
      size: 8
    section header 7 at offset 13096 == 3328
      type: 8 / unknown
      flags: 3 / write, alloc, 
      address: 80000180
      offset: 0
      size: 38
    section header 8 at offset 13160 == 3368
      type: 1 / program data
      flags: 30 / merge, asciz strings, 
      address: 0
      offset: 1178
      size: 11
    section header 9 at offset 13224 == 33a8
      type: 1 / program data
      flags: 0 / 
      address: 0
      offset: 1189
      size: eff
    section header 10 at offset 13288 == 33e8
      type: 1 / program data
      flags: 0 / 
      address: 0
      offset: 2088
      size: 2b9
    section header 11 at offset 13352 == 3428
      type: 1 / program data
      flags: 0 / 
      address: 0
      offset: 2341
      size: 30
    section header 12 at offset 13416 == 3468
      type: 1 / program data
      flags: 0 / 
      address: 0
      offset: 2371
      size: 20
    section header 13 at offset 13480 == 34a8
      type: 1 / program data
      flags: 0 / 
      address: 0
      offset: 2391
      size: 298
    section header 14 at offset 13544 == 34e8
      type: 1 / program data
      flags: 30 / merge, asciz strings, 
      address: 0
      offset: 2629
      size: 63b
    section header 15 at offset 13608 == 3528
      type: 2 / symbol table
      flags: 0 / 
      address: 0
      offset: 2c68
      size: 330
    section header 16 at offset 13672 == 3568
      type: 3 / string table
      flags: 0 / 
      address: 0
      offset: 2f98
      size: 125
    section header 17 at offset 13736 == 35a8
      type: 3 / string table
      flags: 0 / 
      address: 0
      offset: 30bd
      size: ab
    elf image has 34 symbols
        symbol         80000000 size                a == main
        symbol         8000000a size               24 == deregister_tm_clones
        symbol         8000002e size               2c == register_tm_clones
        symbol         8000005a size               42 == __do_global_dtors_aux
        symbol         8000009c size               34 == frame_dummy
        symbol         800000d0 size               12 == _start
        symbol         800000e2 size               16 == rvos_print_text
        symbol         800000f8 size               10 == rvos_rand
        symbol         80000108 size               16 == rvos_exit
        symbol         8000011e size               16 == rvos_print_double
        symbol         80000134 size               12 == rvos_gettimeofday
        symbol         80000146 size               22 == rvos_trace_instructions
        symbol         80000168 size                8 == __frame_dummy_init_array_entry
        symbol         80000170 size               10 == __do_global_dtors_aux_fini_array_entry
        symbol         80000180 size                0 == completed.5485
        symbol         80000180 size                8 == __TMC_END__
        symbol         80000188 size               30 == object.5490
      argument 0 is 'tests\tcrash.elf', at vm address 80000260
    vm memory start:                 000001C35ACBE060
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


