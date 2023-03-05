# RISC-V version of an app to prove you can't win at tic-tac-toe.
# This code expects an extern "C" function called rvos_print_text() that can display a string.
# rvos_print_text() will be system-specific, and may send it out a serial port or to an attached display.
# The main entrypoint is called "bamain" and it takes no arguments. Calling bamain will be system-specific.
# useful:
#    https://github.com/riscv-non-isa/riscv-elf-psabi-doc/blob/master/riscv-cc.adoc
#    https://cdn.hackaday.io/files/1654127076987008/kendryte_datasheet_20181011163248_en.pdf

.equ blank_piece,   0
.equ x_piece,       1
.equ o_piece,       2
.equ minimum_score, 2
.equ maximum_score, 9
.equ win_score,     6
.equ lose_score,    4
.equ tie_score,     5
.equ iterations,    1000

.section .sbss,"aw",@nobits

  .align 3
  g_board:
    .zero   9

  .align 3
  g_string_buffer:
    .zero 128

.section .rodata

  .align  3

  .data
  .type   winner_functions, @object
  .size   winner_functions, 72
  winner_functions:
    .dword  _pos0func
    .dword  _pos1func
    .dword  _pos2func
    .dword  _pos3func
    .dword  _pos4func
    .dword  _pos5func
    .dword  _pos6func
    .dword  _pos7func
    .dword  _pos8func

  .align  3
  .microseconds_nl_string:
    .string " microseconds\n"

  .align  3
  .moves_nl_string:
    .string " moves\n"

  .align  3
  .iterations_nl_string:
    .string " iterations\n"

  .align  3
  .running_string:
    .string "starting...\n"

/* bamain */

.text
.align 3

.ifdef MAIXDUINO
  .globl bamain
  .type bamain, @function
  bamain:
.else
  .globl main
  .type main, @function
  main:
.endif
        .cfi_startproc
        addi    sp, sp, -128
        sd      ra, 16(sp)
        sd      s0, 24(sp)
        sd      s1, 32(sp)
        sd      s2, 40(sp)
        sd      s3, 48(sp)
        sd      s4, 56(sp)
        sd      s5, 64(sp)
        sd      s6, 72(sp)
        sd      s7, 80(sp)
        sd      s8, 88(sp)
        sd      s9, 96(sp)
        sd      s10, 104(sp)
        sd      s11, 112(sp)

        lla     a0, .running_string
        jal     rvos_print_text

        mv      s1, zero             # global move count -- # of board positions examined

        # the k210 CPU doesn't implement rdtime. clock() works, but creates a c-runtime dependency
        rdcycle s3                   # remember the starting time in s3

        mv      a0, zero             # run with a move at position 0
        jal     run_minmax
        add     s1, a0, s1

        li      a0, 1                # run with a move at position 1
        jal     run_minmax
        add     s1, a0, s1

        li      a0, 4                # run with a move at position 4
        jal     run_minmax
        add     s1, a0, s1

        rdcycle a0
        sub     s3, a0, s3           # duration = end - start. 
.ifdef MAIXDUINO
        li      t0, 400              # the k210 runs at 400Mhz
.else
        li      t0, 1                # when running on Windows with clock() as the source
.endif
        div     s3, s3, t0          

        # show the number of moves examined
        mv      a0, s1
        lla     a1, g_string_buffer
        li      a2, 10
        jal     _my_lltoa
        jal     rvos_print_text
        lla     a0, .moves_nl_string
        jal     rvos_print_text

        # show the runtime in microseconds
        mv      a0, s3
        lla     a1, g_string_buffer
        li      a2, 10
        jal     _my_lltoa
        jal     rvos_print_text
        lla     a0, .microseconds_nl_string
        jal     rvos_print_text

        # show the number of iterations
        li      a0, iterations
        lla     a1, g_string_buffer
        li      a2, 10
        jal     _my_lltoa
        jal     rvos_print_text
        lla     a0, .iterations_nl_string
        jal     rvos_print_text

  .ba_exit:
        ld      ra, 16(sp)
        ld      s0, 24(sp)
        ld      s1, 32(sp)
        ld      s2, 40(sp)
        ld      s3, 48(sp)
        ld      s4, 56(sp)
        ld      s5, 64(sp)
        ld      s6, 72(sp)
        ld      s7, 80(sp)
        ld      s8, 88(sp)
        ld      s9, 96(sp)
        ld      s10, 104(sp)
        ld      s11, 112(sp)
        addi    sp, sp, 128
        jr      ra
        .cfi_endproc

/* run_minmax */

.align 3
.type run_minmax, @function
run_minmax:
        .cfi_startproc
        addi    sp, sp, -128
        sd      ra, 16(sp)
        sd      s0, 24(sp)
        sd      s1, 32(sp)
        sd      s2, 40(sp)
        sd      s3, 48(sp)

        mv      s1, a0               # save the move position
        lla     t0, g_board
        mv      s9, t0               # save the board pointer for this thread
        add     t0, a0, t0
        li      t1, x_piece
        sb      t1, (t0)             # make the first move

        lla     s10, winner_functions
        mv      s11, zero            # move count for this thread
        li      s8, iterations       # iteration count

  .run_minmax_next_iteration:
        mv      a3, a0               # first move
        mv      a2, zero             # depth
        li      a1, maximum_score    # beta
        li      a0, minimum_score    # alpha

        jal     minmax_min

        addi    s8, s8, -1
        bne     s8, zero, .run_minmax_next_iteration

        add     t0, s9, s1           # restore a 0 to the move position on the board
        sb      zero, (t0)

  .run_minmax_exit:
        mv      a0, s11
        ld      ra, 16(sp)
        ld      s0, 24(sp)
        ld      s1, 32(sp)
        ld      s2, 40(sp)
        ld      s3, 48(sp)
        addi    sp, sp, 128
        jr      ra
        .cfi_endproc

/* minmax_max */

.align 3
.type minmax_max, @function
minmax_max:
  /*
        a0:   alpha
        a1:   beta
        a2:   depth
        a3:   move
        s0:   alpha local
        s1:   beta local
        s2:   *unused* and not saved
        s3:   *unused* and not saved
        s4:   next depth
        s5:   value local variable
        s6:   loop local variable i
        s7:   the piece to move
        s8:   global iteration count
        s9:   global the board for this thread
        s10:  global winner function table
        s11:  global move count for this thread
  */
        .cfi_startproc
        addi    sp, sp, -128
        sd      ra, 16(sp)
        sd      s0, 24(sp)
        sd      s1, 32(sp)
        sd      s4, 56(sp)         # save space for s2 and s3 if they are used in the future
        sd      s5, 64(sp)
        sd      s6, 72(sp)
        sd      s7, 80(sp)

        addi    s11, s11, 1        # increment move count
        mv      s0, a0             # save alpha

        li      t0, 3              # only check for a winner if > 4 moves taken so far
        ble     a2, t0, .minmax_max_skip_winner

        li      a0, o_piece        # call the winner function for the most recent move
        slli    t0, a3, 3          # function offset is 8 * the move position
        add     t0, t0, s10
        ld      t0, (t0)
        jalr    ra, t0

        li      t0, o_piece
        li      s5, lose_score
        beq     a0, t0, .minmax_max_loadv_done

  .minmax_max_skip_winner:
        mv      s1, a1             # save beta
        addi    s4, a2, 1          # next depth
        li      s7, x_piece        # making x moves below
        li      s5, minimum_score  # min because we're maximizing
        li      s6, -1             # start the loop with I at -1
        li      t6, 8              # t6 is a global constant of 8

  .minmax_max_loop:                # loop over all possible next moves 0..8
        beq     s6, t6, .minmax_max_loadv_done            
        addi    s6, s6, 1

        add     t1, s6, s9         # is this move position free?
        lbu     t0, (t1)
        bne     t0, zero, .minmax_max_loop

        sb      s7, (t1)           # make the move

        mv      a0, s0             # recurse to the min. alpha
        mv      a1, s1             # beta
        mv      a2, s4             # next depth
        mv      a3, s6             # next move
        jal     minmax_min

        add     t1, s6, s9         # restore a 0 to the last move position
        sb      zero, (t1)        

        li      t0, win_score      # can't do better than winning when maximizing
        beq     a0, t0, .minmax_max_done  

        ble     a0, s5, .minmax_max_skip_value  # compare score with value
        mv      s5, a0             # update value with the new high score

  .minmax_max_skip_value:
        ble     s5, s0, .minmax_max_skip_alpha   # compare value with alpha
        mv      s0, s5             # update alpha with value

  .minmax_max_skip_alpha:
        blt     s0, s1, .minmax_max_loop  # alpha pruning

  .minmax_max_loadv_done:
        mv      a0, s5             # return value

  .minmax_max_done:
        ld      ra, 16(sp)
        ld      s0, 24(sp)
        ld      s1, 32(sp)
        ld      s4, 56(sp)
        ld      s5, 64(sp)
        ld      s6, 72(sp)
        ld      s7, 80(sp)
        addi    sp, sp, 128
        jr      ra
        .cfi_endproc

/* minmax_min */

.align 3
.type minmax_min, @function
minmax_min:
  /*
        a0:   alpha
        a1:   beta
        a2:   depth
        a3:   move
        s0:   alpha local
        s1:   beta local
        s2:   *unused* and not saved
        s3:   *unused* and not saved
        s4:   next depth
        s5:   value local variable
        s6:   loop local variable i
        s7:   the piece to move
        s8:   global iteration count
        s9:   global the board for this thread
        s10:  global winner function table
        s11:  global move count for this thread
  */
        .cfi_startproc
        addi    sp, sp, -128
        sd      ra, 16(sp)
        sd      s0, 24(sp)
        sd      s1, 32(sp)
        sd      s4, 56(sp)         # save space for s2 and s3 if they are used in the future
        sd      s5, 64(sp)
        sd      s6, 72(sp)
        sd      s7, 80(sp)

        addi    s11, s11, 1        # increment move count
        mv      s0, a0             # save alpha

        li      t0, 3              # only check for a winner if > 4 moves taken so far
        ble     a2, t0, .minmax_min_skip_winner
        
        li      a0, x_piece        # call the winner function for the most recent move
        slli    t0, a3, 3          # function offset is 8 * the move position
        add     t0, t0, s10
        ld      t0, (t0)
        jalr    ra, t0

        li      t0, x_piece
        li      s5, win_score
        beq     a0, t0, .minmax_min_loadv_done

        li      t0, 8
        li      s5, tie_score
        beq     a2, t0, .minmax_min_loadv_done
        
  .minmax_min_skip_winner:
        mv      s1, a1             # save beta
        addi    s4, a2, 1          # next depth
        li      s7, o_piece        # making o moves below
        li      s5, maximum_score  # max because we're minimizing
        li      s6, -1             # start the loop with I at -1
        li      t6, 8              # t6 is a global constant of 8

  .minmax_min_loop:                # loop over all possible next moves 0..8
        beq     s6, t6, .minmax_min_loadv_done
        addi    s6, s6, 1

        add     t1, s6, s9         # is this move position free?
        lbu     t0, (t1)
        bne     t0, zero, .minmax_min_loop

        sb      s7, (t1)           # make the move

        mv      a0, s0             # recurse to the max
        mv      a1, s1
        mv      a2, s4
        mv      a3, s6
        jal     minmax_max

        add     t1, s6, s9         # restore a 0 to the last move position
        sb      zero, (t1)        

        li      t0, lose_score     # can't do better than losing when minimizing
        beq     a0, t0, .minmax_min_done  

        bge     a0, s5, .minmax_min_skip_value  # compare score with value
        mv      s5, a0             # update value with the new low score

  .minmax_min_skip_value:
        bge     s5, s1, .minmax_min_skip_beta   # compare value with beta
        mv      s1, s5             # update beta with value

  .minmax_min_skip_beta:
        bgt     s1, s0, .minmax_min_loop  # beta pruning

  .minmax_min_loadv_done:
        mv      a0, s5             # return value

  .minmax_min_done:
        ld      ra, 16(sp)
        ld      s0, 24(sp)
        ld      s1, 32(sp)
        ld      s4, 56(sp)
        ld      s5, 64(sp)
        ld      s6, 72(sp)
        ld      s7, 80(sp)
        addi    sp, sp, 128
        jr      ra
        .cfi_endproc

/* _pos0func */

.text
  .align 3
.type _pos0func, @function
_pos0func:
        .cfi_startproc
        mv      t2, a0
        lbu     t0, 1(s9)
        lbu     t1, 2(s9)
        and     a0, t0, a0
        and     a0, t1, a0
        bne     a0, zero, .pos0_func_return

        lbu     t0, 3(s9)
        lbu     t1, 6(s9)
        and     a0, t0, t2
        and     a0, t1, a0
        bne     a0, zero, .pos0_func_return

        lbu     t0, 4(s9)
        lbu     t1, 8(s9)
        and     a0, t0, t2
        and     a0, t1, a0

  .pos0_func_return:
        jr      ra
        .cfi_endproc

/* _pos1func */

.type _pos1func, @function
_pos1func:
        .cfi_startproc
        mv      t2, a0
        lbu     t0, 0(s9)
        lbu     t1, 2(s9)
        and     a0, t0, a0
        and     a0, t1, a0
        bne     a0, zero, .pos1_func_return

        lbu     t0, 4(s9)
        lbu     t1, 7(s9)
        and     a0, t0, t2
        and     a0, t1, a0

  .pos1_func_return:
        jr      ra
        .cfi_endproc

/* _pos2func */

.type _pos2func, @function
_pos2func:
        .cfi_startproc
        mv      t2, a0
        lbu     t0, 0(s9)
        lbu     t1, 1(s9)
        and     a0, t0, a0
        and     a0, t1, a0
        bne     a0, zero, .pos2_func_return

        lbu     t0, 5(s9)
        lbu     t1, 8(s9)
        and     a0, t0, t2
        and     a0, t1, a0
        bne     a0, zero, .pos2_func_return

        lbu     t0, 4(s9)
        lbu     t1, 6(s9)
        and     a0, t0, t2
        and     a0, t1, a0

  .pos2_func_return:
        jr      ra
        .cfi_endproc

/* _pos3func */

.type _pos3func, @function
_pos3func:
        .cfi_startproc
        mv      t2, a0
        lbu     t0, 0(s9)
        lbu     t1, 6(s9)
        and     a0, t0, a0
        and     a0, t1, a0
        bne     a0, zero, .pos3_func_return

        lbu     t0, 4(s9)
        lbu     t1, 5(s9)
        and     a0, t0, t2
        and     a0, t1, a0

  .pos3_func_return:
        jr      ra
        .cfi_endproc

/* _pos4func */

.type _pos4func, @function
_pos4func:
        .cfi_startproc
        mv      t2, a0
        lbu     t0, 0(s9)
        lbu     t1, 8(s9)
        and     a0, t0, a0
        and     a0, t1, a0
        bne     a0, zero, .pos4_func_return

        lbu     t0, 2(s9)
        lbu     t1, 6(s9)
        and     a0, t0, t2
        and     a0, t1, a0
        bne     a0, zero, .pos4_func_return

        lbu     t0, 1(s9)
        lbu     t1, 7(s9)
        and     a0, t0, t2
        and     a0, t1, a0
        bne     a0, zero, .pos4_func_return

        lbu     t0, 3(s9)
        lbu     t1, 5(s9)
        and     a0, t0, t2
        and     a0, t1, a0

  .pos4_func_return:
        jr      ra
        .cfi_endproc

/* _pos5func */

.type _pos5func, @function
_pos5func:
        .cfi_startproc
        mv      t2, a0
        lbu     t0, 3(s9)
        lbu     t1, 4(s9)
        and     a0, t0, a0
        and     a0, t1, a0
        bne     a0, zero, .pos5_func_return

        lbu     t0, 2(s9)
        lbu     t1, 8(s9)
        and     a0, t0, t2
        and     a0, t1, a0

  .pos5_func_return:
        jr      ra
        .cfi_endproc

/* _pos6func */

.type _pos6func, @function
_pos6func:
        .cfi_startproc
        mv      t2, a0
        lbu     t0, 0(s9)
        lbu     t1, 3(s9)
        and     a0, t0, a0
        and     a0, t1, a0
        bne     a0, zero, .pos6_func_return

        lbu     t0, 2(s9)
        lbu     t1, 4(s9)
        and     a0, t0, t2
        and     a0, t1, a0
        bne     a0, zero, .pos6_func_return

        lbu     t0, 7(s9)
        lbu     t1, 8(s9)
        and     a0, t0, t2
        and     a0, t1, a0

  .pos6_func_return:
        jr      ra
        .cfi_endproc

/* _pos7func */

.type _pos7func, @function
_pos7func:
        .cfi_startproc
        mv      t2, a0
        lbu     t0, 1(s9)
        lbu     t1, 4(s9)
        and     a0, t0, a0
        and     a0, t1, a0
        bne     a0, zero, .pos7_func_return

        lbu     t0, 6(s9)
        lbu     t1, 8(s9)
        and     a0, t0, t2
        and     a0, t1, a0

  .pos7_func_return:
        jr      ra
        .cfi_endproc

/* _pos8func */

.type _pos8func, @function
_pos8func:
        .cfi_startproc
        mv      t2, a0
        lbu     t0, 0(s9)
        lbu     t1, 4(s9)
        and     a0, t0, a0
        and     a0, t1, a0
        bne     a0, zero, .pos8_func_return

        lbu     t0, 2(s9)
        lbu     t1, 5(s9)
        and     a0, t0, t2
        and     a0, t1, a0
        bne     a0, zero, .pos8_func_return

        lbu     t0, 6(s9)
        lbu     t1, 7(s9)
        and     a0, t0, t2
        and     a0, t1, a0

  .pos8_func_return:
        jr      ra
        .cfi_endproc

/* _my_lltoa a0: 8-byte signed integer, a1: 8-bit ascii string buffer, a2: base */

.type _my_lltoa, @function
_my_lltoa:
        .cfi_startproc
        li      t1, 9
        bne     a0, zero, .my_lltoa_not_zero
        li      t0, '0'
        sb      t0, (a1)
        sb      zero, 1(a1)
        j       .my_lltoa_exit

  .my_lltoa_not_zero:
        li      t2, 0           # offset into the string
        mv      t6, zero        # default to unsigned
        li      t0, 10          # negative numbers only exist for base 10
        bne     a2, t0, .my_lltoa_digit_loop 
        li      t0, 0x8000000000000000
        and     t0, a0, t0
        beq     t0, zero, .my_lltoa_digit_loop
        li      t6, 1           # it's negative
        neg     a0, a0          # this is just sub a0, zero, a0

  .my_lltoa_digit_loop:
        beq     a0, zero, .my_lltoa_digits_done
        rem     t0, a0, a2
        bgt     t0, t1, .my_lltoa_more_than_nine
        addi    t0, t0, '0'
        j       .my_lltoa_after_base
  .my_lltoa_more_than_nine:
        addi    t0, t0, 'a' - 10
  .my_lltoa_after_base:
        add     t3, a1, t2
        sb      t0, (t3)
        addi    t2, t2, 1
        div     a0, a0, a2
        j       .my_lltoa_digit_loop

  .my_lltoa_digits_done:
        beq     t6, zero, .my_lltoa_no_minus
        li      t0, '-'
        add     t3, a1, t2
        sb      t0, (t3)
        addi    t2, t2, 1

  .my_lltoa_no_minus:
        add     t3, a1, t2      # null-terminate the string
        sb      zero, (t3)

        mv      t4, a1          # reverse the string. t4 = left
        add     t5, a1, t2      # t5 = right
        addi    t5, t5, -1
  .my_lltoa_reverse_next:
        bge     t4, t5, .my_lltoa_exit
        lbu     t0, (t4)
        lbu     t1, (t5)
        sb      t0, (t5)
        sb      t1, (t4)
        addi    t4, t4, 1
        addi    t5, t5, -1
        j       .my_lltoa_reverse_next

  .my_lltoa_exit:
        mv      a0, a1
        jr      ra
        .cfi_endproc

