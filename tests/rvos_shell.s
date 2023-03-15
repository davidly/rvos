# This shell is for running apps generated by the BA basic compiler in the RISC-V OS emulator rvos.
# This abstraction of _start and rvos_print_text allows the same apps to run on
# physical hardware: the Kendryte K210 Sipeed Maixduino Board.

.text
.align 3

.ifdef DEFINESTART
.globl _start
.type _start, @function
_start:
        .cfi_startproc

        # arguments are up the stack just like Linux

        ld      a0, 0(sp)
        addi    a1, sp, 8

        jal     main

        li      a7, 93 # linux exit function for risc-v
        ecall
        .cfi_endproc
.endif

.globl rvos_print_text
.type rvos_print_text, @function
rvos_print_text:
        .cfi_startproc
        addi    sp, sp, -32
        sd      ra, 16(sp)

        li      a7, 0x2004 # rvos_sys_print_text
        ecall

        ld      ra, 16(sp)
        addi    sp, sp, 32
        jr      ra
        .cfi_endproc

.globl rvos_rand
.type rvos_rand, @function
rvos_rand:
        .cfi_startproc
        addi    sp, sp, -32
        sd      ra, 16(sp)

        li      a7, 0x2000 # rvos_sys_rand
        ecall

        ld      ra, 16(sp)
        addi    sp, sp, 32
        jr      ra
        .cfi_endproc

.globl rvos_exit
.type rvos_exit, @function
rvos_exit:
        .cfi_startproc
        addi    sp, sp, -32
        sd      ra, 16(sp)

        li      a7, 0x2003 # rvos_sys_exit
        ecall

        ld      ra, 16(sp)
        addi    sp, sp, 32
        jr      ra
        .cfi_endproc

.globl rvos_print_double
.type rvos_print_double, @function
rvos_print_double:
        .cfi_startproc
        addi    sp, sp, -32
        sd      ra, 16(sp)

        li      a7, 0x2001  # rvos_sys_print_double
        ecall

        ld      ra, 16(sp)
        addi    sp, sp, 32
        jr      ra
        .cfi_endproc

.globl rvos_gettimeofday
.type rvos_gettimeofday, @function
rvos_gettimeofday:
        .cfi_startproc
        addi    sp, sp, -32
        sd      ra, 16(sp)

        li      a7, 169
        ecall

        ld      ra, 16(sp)
        addi    sp, sp, 32
        jr      ra
        .cfi_endproc

.globl rvos_trace_instructions
.type rvos_trace_instructions, @function
rvos_trace_instructions:
        .cfi_startproc
        addi    sp, sp, -32
        sd      ra, 16(sp)

        li      a7, 0x2002  # rvos_sys_trace_instructions
        ecall

        ld      ra, 16(sp)
        addi    sp, sp, 32
        jr      ra
        .cfi_endproc

# function to set sp to invalid values for testing
.globl rvos_sp_add
.type rvos_sp_add, @function
rvos_sp_add:
        .cfi_startproc
        add     sp, sp, a0
        jr      ra
        .cfi_endproc

