# RISC-V version of an app to prove you can't win at tic-tac-toe.
# This version unrolls the for loop over all board positions, like g++ does for the c++ version.
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
.equ iterations,    1

.section .sbss,"aw",@nobits
  .p2align 4
  g_string_buffer:
    .zero 128

.section .data
  .p2align 6 # Lichee Pi 4A cache line size is 64
    board0: .byte 1,0,0,0,0,0,0,0,0
  .p2align 6 
    board1: .byte 0,1,0,0,0,0,0,0,0
  .p2align 6 
    board4: .byte 0,0,0,0,1,0,0,0,0

  .p2align 6 
    resultthread1:   .quad 0
    resultthread4:   .quad 0
    pthread1:        .quad 0
    pthread4:        .quad 0
    iterationstorun: .quad 0
    rdtimefreq:      .quad 100000 # default

.section .rodata
  .p2align  4
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

  .p2align  4
    .milliseconds_nl_string: .string "%llu milliseconds\n"
    .running_string: .string "starting...\n"
    .moves_nl_string: .string "%llu moves\n"
    .iterations_nl_string: .string "%llu iterations\n"
    .progress_string: .string "progress %llu\n"
    .freq_string: .string "/proc/device-tree/cpus/timebase-frequency"
    .freq_open_mode_string: .string "rb"

.text
  .p2align 4
  .globl main
  .type main, @function
  main:
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

        li      s4, iterations       # global max iteration count in s4
        lla     t0, iterationstorun
        sd      a0, (t0)
        
        li      t0, 2
        blt     a0, t0, .main_start_test  # if no iteration command-line argument, use the default

        li      t0, 8                # get the second argument
        add     t0, t0, a1           # offset of argv[1]
        ld      a0, (t0)             # load argv[1] -- an ascii string
        mv      a1, zero
        li      a2, 10
        jal     strtoull             # convert the ascii string iteration count to a number
        mv      s4, a0               # update the global max iteration count
        lla     t0, iterationstorun
        sd      a0, (t0)

  .main_start_test:
        # get the rdtime frequency divisor
        lla     a0, .freq_string
        lla     a1, .freq_open_mode_string
        jal     fopen
        beq     a0, zero, .main_no_freq
        mv      s1, a0
        lla     a0, rdtimefreq
        li      a1, 1
        li      a2, 8
        mv      a3, s1
        jal     fread                # this may read just 4 bytes, and that's OK
        mv      s2, a0
        mv      a0, s1
        jal     fclose

  .main_no_freq:
        lla     a0, .running_string
        jal     printf

        # first run single-threaded for the 3 unique board positions

        mv      s1, zero             # global move count -- # of board positions examined
        rdtime  s3

        mv      a0, zero             # run with a move at position 0
        jal     run_minmax
        add     s1, a0, s1

        li      a0, 1                # run with a move at position 1
        jal     run_minmax
        add     s1, a0, s1

        li      a0, 4                # run with a move at position 4
        jal     run_minmax
        add     s1, a0, s1

        jal     show_run_stats

        # now run multi-threaded

        rdtime  s3
        jal     solve_threaded
        beq     a0, zero, .main_exit # likely the machine doesn't support multiple threads
        mv      s1, a0               # global move count
        jal     show_run_stats

  .main_exit:
        li      a0, 0
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

# a0 returns 0 on failure or number of board positions examined
.type solve_threaded, @function
solve_threaded:
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

        # solve board 1 in a new thread       
        lla     a0, pthread1
        mv      a1, zero
        lla     a2, run_minmax
        li      a3, 1
        jal     pthread_create

        # if it failed it's probably on rvos or some other environment with no thread support
        beq     a0, zero, solve_threaded_next
        mv      a0, zero
        j       solve_threaded_done

  solve_threaded_next:
        # solve board 4
        lla     a0, pthread4
        mv      a1, zero
        lla     a2, run_minmax
        li      a3, 4
        jal     pthread_create

        # solve board 0 on this thread
        mv      a0, zero             # run with a move at position 0
        jal     run_minmax
        mv      s6, a0

        # wait for board 1 to complete
        lla     a0, pthread1
        ld      a0, (a0)
        lla     a1, resultthread1
        jal     pthread_join
        lla     a1, resultthread1
        ld      a1, (a1)
        add     s6, s6, a1

        # wait for board 4 to complete
        lla     a0, pthread4
        ld      a0, (a0)
        lla     a1, resultthread4
        jal     pthread_join
        lla     a1, resultthread4
        ld      a1, (a1)
        add     a0, s6, a1
        
  solve_threaded_done:
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

# s1: # of moves examined
# s3: start time
# s4: # of iterations
.type show_run_stats, @function
show_run_stats:
        .cfi_startproc
        addi    sp, sp, -16
        sd      ra, 0(sp)

        li      t2, 1000000          # should be 1000, but LPi4A lies, so rvos lies too        
        rdtime  a0
        sub     s3, a0, s3           # duration = end - start.
        mul     s3, s3, t2           # want result in milliseconds

        lla     t0, rdtimefreq
        ld      t0, (t0)
        div     s3, s3, t0           # diff in units*1000 / freq = ms

        # show the number of moves examined
        lla     a0, .moves_nl_string
        mv      a1, s1
        jal     printf

        # show the runtime in milliseconds
        lla     a0, .milliseconds_nl_string
        mv      a1, s3
        jal     printf

        # show the number of iterations
        lla     a0, .iterations_nl_string
        mv      a1, s4
        jal     printf

        ld      ra,0(sp)
        addi    sp, sp, 16
        jr      ra
        .cfi_endproc

# a0: the first move 0, 1, 4
# a0: on return, the # of moves analyzed
.type run_minmax, @function
run_minmax:
        .cfi_startproc
        addi    sp, sp, -64
        sd      ra, 16(sp)
        sd      s0, 24(sp)
        sd      s1, 32(sp)
        sd      s2, 40(sp)
        sd      s3, 48(sp)

        lla     t0, iterationstorun
        ld      s4, (t0)

        li      t4, x_piece          # t4 contains x_piece for the entire run
        li      t5, o_piece          # t5 contains o_piece for the entire run
        li      t6, 8                # t6 contains 8 for the entire run
        li      s5, win_score        # s5 contains win_score for the entire run
        li      s6, lose_score       # s6 contains lose_score for the entire run
        li      a7, 3                # a7 contains 3 for the entire run

        # load t0 with the board to use
        bne     a0, zero, _runmm_try1
        lla     s9, board0
        j       _runmm_for

  _runmm_try1:
        li      t0, 1
        bne     a0, t0, _runmm_try4
        lla     s9, board1
        j       _runmm_for

  _runmm_try4:                       # force any other value to use board4
        lla     s9, board4

  _runmm_for:
        mv      s3, a0               # save the move position
        lla     s10, winner_functions # save the global winner function table
        mv      s11, zero            # global move count for this thread
        mv      s7, zero             # global depth for this thread
        mv      s8, s4               # iteration count

  .run_minmax_next_iteration:
        li      s1, maximum_score    # beta
        li      s0, minimum_score    # alpha
        jal     minmax_min

        addi    s8, s8, -1
        bne     s8, zero, .run_minmax_next_iteration

  .run_minmax_exit:
        mv      a0, s11
        ld      ra, 16(sp)
        ld      s0, 24(sp)
        ld      s1, 32(sp)
        ld      s2, 40(sp)
        ld      s3, 48(sp)
        addi    sp, sp, 64
        jr      ra
        .cfi_endproc

.type minmax_max, @function
minmax_max:
  /* same for minmax_max and minmax_min:
        s0:   alpha argument and local
        s1:   beta argument and local
        s2:   value local variable
        s3:   i loop local variable / move argument
        s4:   global max iteration count
        s5:   global constant win_score
        s6:   global constant lose_score
        s7:   global depth
        s8:   global iteration count
        s9:   global the board for this thread
        s10:  global winner function table
        s11:  global move count for this thread
        t3:   argument and result for scoring procs
        t4:   the constant x_piece (3)
        t5:   the constant o_piece
        t6:   the constant 8
        a7:   the constant 3
  */
        .cfi_startproc
        addi    sp, sp, -32
        sd      ra, 0(sp)

        addi    s11, s11, 1        # increment move count

        ble     s7, a7, .minmax_max_skip_winner # only check for a winner if > 4 moves taken so far

        li      t3, o_piece        # call the winner function for the most recent move
        slli    t0, s3, 3          # function offset is 8 * the move position
        add     t0, t0, s10
        ld      t0, (t0)
        jalr    ra, t0

        beq     t3, t5, .minmax_max_lose  # if o won, return lose_score

  .minmax_max_skip_winner:
        sd      s0, 8(sp)          # save caller's alpha
        sd      s2, 16(sp)         # save caller's local value
        sd      s3, 24(sp)         # save caller's i variable

        li      s2, minimum_score  # min because we're maximizing
        addi    s7, s7, 1          # next depth

        lbu     t0, 0(s9)
        beq     t0, zero, .minmax_max_move_0

  .minmax_max_try_1:
        lbu     t0, 1(s9)
        beq     t0, zero, .minmax_max_move_1

  .minmax_max_try_2:
        lbu     t0, 2(s9)
        beq     t0, zero, .minmax_max_move_2

  .minmax_max_try_3:
        lbu     t0, 3(s9)
        beq     t0, zero, .minmax_max_move_3

  .minmax_max_try_4:
        lbu     t0, 4(s9)
        beq     t0, zero, .minmax_max_move_4

  .minmax_max_try_5:
        lbu     t0, 5(s9)
        beq     t0, zero, .minmax_max_move_5

  .minmax_max_try_6:
        lbu     t0, 6(s9)
        beq     t0, zero, .minmax_max_move_6

  .minmax_max_try_7:
        lbu     t0, 7(s9)
        beq     t0, zero, .minmax_max_move_7

  .minmax_max_try_8:
        lbu     t0, 8(s9)
        beq     t0, zero, .minmax_max_move_8
        j       .minmax_max_loadv_done

  .minmax_max_move_0:        
        sb      t4, 0(s9)                 # make the x_piece move
        li      s3, 0                     # the move
        jal     minmax_min                # recurse to the min
        sb      zero, 0(s9)               # restore the board position to 0     
        beq     a0, s5, .minmax_max_done  # can't do better than winning when maximizing 
        ble     a0, s2, .minmax_max_try_1 # compare score with value and loop again if no better
        mv      s2, a0                    # update value with the new high score
        bge     a0, s1, .minmax_max_done  # compare value with beta and prune/return if >=
        ble     a0, s0, .minmax_max_try_1 # compare value with alpha and loop if <=
        mv      s0, a0                    # update alpha with value
        j       .minmax_max_try_1

  .minmax_max_move_1:       
        sb      t4, 1(s9)                 
        li      s3, 1                     
        jal     minmax_min                
        sb      zero, 1(s9)               
        beq     a0, s5, .minmax_max_done  
        ble     a0, s2, .minmax_max_try_2
        mv      s2, a0                    
        bge     a0, s1, .minmax_max_done  
        ble     a0, s0, .minmax_max_try_2 
        mv      s0, a0                    
        j       .minmax_max_try_2

  .minmax_max_move_2:       
        sb      t4, 2(s9)                 
        li      s3, 2                     
        jal     minmax_min                
        sb      zero, 2(s9)               
        beq     a0, s5, .minmax_max_done  
        ble     a0, s2, .minmax_max_try_3
        mv      s2, a0                    
        bge     a0, s1, .minmax_max_done  
        ble     a0, s0, .minmax_max_try_3 
        mv      s0, a0                    
        j       .minmax_max_try_3

  .minmax_max_move_3:       
        sb      t4, 3(s9)                 
        li      s3, 3                     
        jal     minmax_min                
        sb      zero, 3(s9)               
        beq     a0, s5, .minmax_max_done  
        ble     a0, s2, .minmax_max_try_4
        mv      s2, a0                    
        bge     a0, s1, .minmax_max_done  
        ble     a0, s0, .minmax_max_try_4 
        mv      s0, a0                    
        j       .minmax_max_try_4

  .minmax_max_move_4:       
        sb      t4, 4(s9)                 
        li      s3, 4                     
        jal     minmax_min                
        sb      zero, 4(s9)               
        beq     a0, s5, .minmax_max_done  
        ble     a0, s2, .minmax_max_try_5
        mv      s2, a0                    
        bge     a0, s1, .minmax_max_done  
        ble     a0, s0, .minmax_max_try_5 
        mv      s0, a0                    
        j       .minmax_max_try_5

  .minmax_max_move_5:       
        sb      t4, 5(s9)                 
        li      s3, 5                     
        jal     minmax_min                
        sb      zero, 5(s9)               
        beq     a0, s5, .minmax_max_done  
        ble     a0, s2, .minmax_max_try_6
        mv      s2, a0                    
        bge     a0, s1, .minmax_max_done  
        ble     a0, s0, .minmax_max_try_6 
        mv      s0, a0                    
        j       .minmax_max_try_6

  .minmax_max_move_6:       
        sb      t4, 6(s9)                 
        li      s3, 6                     
        jal     minmax_min                
        sb      zero, 6(s9)               
        beq     a0, s5, .minmax_max_done  
        ble     a0, s2, .minmax_max_try_7
        mv      s2, a0                    
        bge     a0, s1, .minmax_max_done  
        ble     a0, s0, .minmax_max_try_7 
        mv      s0, a0                    
        j       .minmax_max_try_7

  .minmax_max_move_7:       
        sb      t4, 7(s9)                 
        li      s3, 7                     
        jal     minmax_min                
        sb      zero, 7(s9)               
        beq     a0, s5, .minmax_max_done  
        ble     a0, s2, .minmax_max_try_8
        mv      s2, a0                    
        bge     a0, s1, .minmax_max_done  
        ble     a0, s0, .minmax_max_try_8 
        mv      s0, a0                    
        j       .minmax_max_try_8

  .minmax_max_move_8:       
        sb      t4, 8(s9)                 
        li      s3, 8                     
        jal     minmax_min                
        sb      zero, 8(s9)               
        beq     a0, s5, .minmax_max_done  
        ble     a0, s2, .minmax_max_loadv_done
        mv      s2, a0                    
        bge     a0, s1, .minmax_max_done  

  .minmax_max_loadv_done:
        mv      a0, s2             # return value

  .minmax_max_done:
        addi    s7, s7, -1         # restore depth
        ld      s0, 8(sp)
        ld      s2, 16(sp)
        ld      s3, 24(sp)

  .minmax_max_fast_exit:
        ld      ra, 0(sp)
        addi    sp, sp, 32
        jr      ra

  .minmax_max_lose:
        li      a0, lose_score
        ld      ra, 0(sp)
        addi    sp, sp, 32
        jr      ra

        .cfi_endproc

.type minmax_min, @function
minmax_min:
        .cfi_startproc
        addi    sp, sp, -32
        sd      ra, 0(sp)

        addi    s11, s11, 1        # increment move count

        ble     s7, a7, .minmax_min_skip_winner # only check for a winner if > 4 moves taken so far

        li      t3, x_piece        # call the winner function for the most recent move
        slli    t0, s3, 3          # function offset is 8 * the move position
        add     t0, t0, s10
        ld      t0, (t0)
        jalr    ra, t0

        beq     t3, t4, .minmax_min_win   # if x won, return win_score
        beq     s7, t6, .minmax_min_tie   # if at the max depth, it's a tie
        
  .minmax_min_skip_winner:
        sd      s1, 8(sp)          # save caller's beta local
        sd      s2, 16(sp)         # save caller's value local
        sd      s3, 24(sp)         # save caller's i loop local

        li      s2, maximum_score  # max because we're minimizing
        addi    s7, s7, 1          # next depth

        lbu     t0, 0(s9)
        beq     t0, zero, .minmax_min_move_0

  .minmax_min_try_1:
        lbu     t0, 1(s9)
        beq     t0, zero, .minmax_min_move_1

  .minmax_min_try_2:
        lbu     t0, 2(s9)
        beq     t0, zero, .minmax_min_move_2

  .minmax_min_try_3:
        lbu     t0, 3(s9)
        beq     t0, zero, .minmax_min_move_3

  .minmax_min_try_4:
        lbu     t0, 4(s9)
        beq     t0, zero, .minmax_min_move_4

  .minmax_min_try_5:
        lbu     t0, 5(s9)
        beq     t0, zero, .minmax_min_move_5

  .minmax_min_try_6:
        lbu     t0, 6(s9)
        beq     t0, zero, .minmax_min_move_6

  .minmax_min_try_7:
        lbu     t0, 7(s9)
        beq     t0, zero, .minmax_min_move_7

  .minmax_min_try_8:
        lbu     t0, 8(s9)
        beq     t0, zero, .minmax_min_move_8
        j       .minmax_min_loadv_done

  .minmax_min_move_0:        
        sb      t5, 0(s9)                 # make the o_piece move
        li      s3, 0                     # move is in position 0
        jal     minmax_max                # recurse to the max
        sb      zero, 0(s9)               # restore the board position to 0
        beq     a0, s6, .minmax_min_done  # can't do better than losing when minimizing
        bge     a0, s2, .minmax_min_try_1 # compare score with value and loop again if no worse
        mv      s2, a0                    # update value with the new low score
        ble     a0, s0, .minmax_min_done  # compare value with alpha and prune/return if <=
        bge     a0, s1, .minmax_min_try_1 # compare value with beta and loop if >=
        mv      s1, a0                    # update beta with value
        j       .minmax_min_try_1

  .minmax_min_move_1:        
        sb      t5, 1(s9)
        li      s3, 1
        jal     minmax_max
        sb      zero, 1(s9)        
        beq     a0, s6, .minmax_min_done
        bge     a0, s2, .minmax_min_try_2
        mv      s2, a0                   
        ble     a0, s0, .minmax_min_done 
        bge     a0, s1, .minmax_min_try_2
        mv      s1, a0                   
        j       .minmax_min_try_2

  .minmax_min_move_2:        
        sb      t5, 2(s9)
        li      s3, 2
        jal     minmax_max
        sb      zero, 2(s9)        
        beq     a0, s6, .minmax_min_done
        bge     a0, s2, .minmax_min_try_3
        mv      s2, a0                   
        ble     a0, s0, .minmax_min_done 
        bge     a0, s1, .minmax_min_try_3
        mv      s1, a0                   
        j       .minmax_min_try_3

  .minmax_min_move_3:        
        sb      t5, 3(s9)
        li      s3, 3
        jal     minmax_max
        sb      zero, 3(s9)        
        beq     a0, s6, .minmax_min_done
        bge     a0, s2, .minmax_min_try_4
        mv      s2, a0                   
        ble     a0, s0, .minmax_min_done 
        bge     a0, s1, .minmax_min_try_4
        mv      s1, a0                   
        j       .minmax_min_try_4

  .minmax_min_move_4:        
        sb      t5, 4(s9)
        li      s3, 4
        jal     minmax_max
        sb      zero, 4(s9)        
        beq     a0, s6, .minmax_min_done
        bge     a0, s2, .minmax_min_try_5
        mv      s2, a0                   
        ble     a0, s0, .minmax_min_done 
        bge     a0, s1, .minmax_min_try_5
        mv      s1, a0                   
        j       .minmax_min_try_5

  .minmax_min_move_5:        
        sb      t5, 5(s9)
        li      s3, 5
        jal     minmax_max
        sb      zero, 5(s9)        
        beq     a0, s6, .minmax_min_done
        bge     a0, s2, .minmax_min_try_6
        mv      s2, a0                   
        ble     a0, s0, .minmax_min_done 
        bge     a0, s1, .minmax_min_try_6
        mv      s1, a0                   
        j       .minmax_min_try_6

  .minmax_min_move_6:        
        sb      t5, 6(s9)
        li      s3, 6
        jal     minmax_max
        sb      zero, 6(s9)        
        beq     a0, s6, .minmax_min_done
        bge     a0, s2, .minmax_min_try_7
        mv      s2, a0                   
        ble     a0, s0, .minmax_min_done 
        bge     a0, s1, .minmax_min_try_7
        mv      s1, a0                   
        j       .minmax_min_try_7

  .minmax_min_move_7:        
        sb      t5, 7(s9)
        li      s3, 7
        jal     minmax_max
        sb      zero, 7(s9)        
        beq     a0, s6, .minmax_min_done
        bge     a0, s2, .minmax_min_try_8
        mv      s2, a0                   
        ble     a0, s0, .minmax_min_done 
        bge     a0, s1, .minmax_min_try_8
        mv      s1, a0                   
        j       .minmax_min_try_8

  .minmax_min_move_8:        
        sb      t5, 8(s9)
        li      s3, 8
        jal     minmax_max
        sb      zero, 8(s9)        
        beq     a0, s6, .minmax_min_done
        bge     a0, s2, .minmax_min_loadv_done
        mv      s2, a0                   
        ble     a0, s0, .minmax_min_done 

  .minmax_min_loadv_done:
        mv      a0, s2             # return value

  .minmax_min_done:
        addi    s7, s7, -1         # restore depth
        ld      s1, 8(sp)
        ld      s2, 16(sp)
        ld      s3, 24(sp)

  .minmax_min_fast_exit:
        ld      ra, 0(sp)
        addi    sp, sp, 32
        jr      ra

  .minmax_min_win:
        li      a0, win_score
        ld      ra, 0(sp)
        addi    sp, sp, 32
        jr      ra

  .minmax_min_tie:
        li      a0, tie_score
        ld      ra, 0(sp)
        addi    sp, sp, 32
        jr      ra

        .cfi_endproc

# all _posXfunc functions take the piece argument in t3 and return the winner piece in t3.

.if 1

# this version of pos functions with AND results in more instructions and slower rvos
# emulator times, but it's faster on physical hardware due to less branching than the
# more obvious load then branch for each board position implementation.

.type _pos0func, @function
_pos0func:
        .cfi_startproc
        mv      t2, t3
        lbu     t0, 1(s9)
        and     t3, t0, t3
        lbu     t1, 2(s9)
        and     t3, t1, t3
        bne     t3, zero, .pos0_func_return

        lbu     t0, 3(s9)
        and     t3, t0, t2
        lbu     t1, 6(s9)
        and     t3, t1, t3
        bne     t3, zero, .pos0_func_return

        lbu     t0, 4(s9)
        and     t3, t0, t2
        lbu     t1, 8(s9)
        and     t3, t1, t3

  .pos0_func_return:
        jr      ra
        .cfi_endproc

.type _pos1func, @function
_pos1func:
        .cfi_startproc
        mv      t2, t3
        lbu     t0, 0(s9)
        and     t3, t0, t3
        lbu     t1, 2(s9)
        and     t3, t1, t3
        bne     t3, zero, .pos1_func_return

        lbu     t0, 4(s9)
        and     t3, t0, t2
        lbu     t1, 7(s9)
        and     t3, t1, t3

  .pos1_func_return:
        jr      ra
        .cfi_endproc

.type _pos2func, @function
_pos2func:
        .cfi_startproc
        mv      t2, t3
        lbu     t0, 0(s9)
        and     t3, t0, t3
        lbu     t1, 1(s9)
        and     t3, t1, t3
        bne     t3, zero, .pos2_func_return

        lbu     t0, 5(s9)
        and     t3, t0, t2
        lbu     t1, 8(s9)
        and     t3, t1, t3
        bne     t3, zero, .pos3_func_return

        lbu     t0, 4(s9)
        and     t3, t0, t2
        lbu     t1, 6(s9)
        and     t3, t1, t3

  .pos2_func_return:
        jr      ra
        .cfi_endproc

.type _pos3func, @function
_pos3func:
        .cfi_startproc
        mv      t2, t3
        lbu     t0, 0(s9)
        and     t3, t0, t3
        lbu     t1, 6(s9)
        and     t3, t1, t3
        bne     t3, zero, .pos3_func_return

        lbu     t0, 4(s9)
        lbu     t1, 5(s9)
        and     t3, t0, t2
        and     t3, t1, t3

  .pos3_func_return:
        jr      ra
        .cfi_endproc

.type _pos4func, @function
_pos4func:
        .cfi_startproc
        mv      t2, t3
        lbu     t0, 0(s9)
        and     t3, t0, t3
        lbu     t1, 8(s9)
        and     t3, t1, t3
        bne     t3, zero, .pos4_func_return

        lbu     t0, 2(s9)
        and     t3, t0, t2
        lbu     t1, 6(s9)
        and     t3, t1, t3
        bne     t3, zero, .pos4_func_return

        lbu     t0, 1(s9)
        and     t3, t0, t2
        lbu     t1, 7(s9)
        and     t3, t1, t3
        bne     t3, zero, .pos4_func_return

        lbu     t0, 3(s9)
        and     t3, t0, t2
        lbu     t1, 5(s9)
        and     t3, t1, t3

  .pos4_func_return:
        jr      ra
        .cfi_endproc

.type _pos5func, @function
_pos5func:
        .cfi_startproc
        mv      t2, t3
        lbu     t0, 3(s9)
        and     t3, t0, t3
        lbu     t1, 4(s9)
        and     t3, t1, t3
        bne     t3, zero, .pos5_func_return

        lbu     t0, 2(s9)
        and     t3, t0, t2
        lbu     t1, 8(s9)
        and     t3, t1, t3

  .pos5_func_return:
        jr      ra
        .cfi_endproc

.type _pos6func, @function
_pos6func:
        .cfi_startproc
        mv      t2, t3
        lbu     t0, 0(s9)
        and     t3, t0, t3
        lbu     t1, 3(s9)
        and     t3, t1, t3
        bne     t3, zero, .pos6_func_return

        lbu     t0, 2(s9)
        and     t3, t0, t2
        lbu     t1, 4(s9)
        and     t3, t1, t3
        bne     t3, zero, .pos6_func_return

        lbu     t0, 7(s9)
        and     t3, t0, t2
        lbu     t1, 8(s9)
        and     t3, t1, t3

  .pos6_func_return:
        jr      ra
        .cfi_endproc

.type _pos7func, @function
_pos7func:
        .cfi_startproc
        mv      t2, t3
        lbu     t0, 1(s9)
        and     t3, t0, t3
        lbu     t1, 4(s9)
        and     t3, t1, t3
        bne     t3, zero, .pos7_func_return

        lbu     t0, 6(s9)
        and     t3, t0, t2
        lbu     t1, 8(s9)
        and     t3, t1, t3

  .pos7_func_return:
        jr      ra
        .cfi_endproc

.type _pos8func, @function
_pos8func:
        .cfi_startproc
        mv      t2, t3
        lbu     t0, 0(s9)
        and     t3, t0, t3
        lbu     t1, 4(s9)
        and     t3, t1, t3
        bne     t3, zero, .pos8_func_return

        lbu     t0, 2(s9)
        and     t3, t0, t2
        lbu     t1, 5(s9)
        and     t3, t1, t3
        bne     t3, zero, .pos8_func_return

        lbu     t0, 6(s9)
        and     t3, t0, t2
        lbu     t1, 7(s9)
        and     t3, t1, t3

  .pos8_func_return:
        jr      ra
        .cfi_endproc

.else

# these run with fewer instructions and are faster on the rvos emulator, but are slower on physical hardware due to extra branching

.text
.type _pos0func, @function
_pos0func:
        .cfi_startproc
        lbu     t0, 1(s9)
        bne     t3, t0, .pos0_func_chk1
        lbu     t0, 2(s9)
        beq     t3, t0, .pos0_func_return

  .pos0_func_chk1:        
        lbu     t0, 3(s9)
        bne     t3, t0, .pos0_func_chk2
        lbu     t0, 6(s9)
        beq     t3, t0, .pos0_func_return

  .pos0_func_chk2:        
        lbu     t0, 4(s9)
        bne     t3, t0, .pos0_func_fail
        lbu     t0, 8(s9)
        beq     t3, t0, .pos0_func_return

  .pos0_func_fail:
        mv      t3, zero        

  .pos0_func_return:
        jr      ra
        .cfi_endproc

.type _pos1func, @function
_pos1func:
        .cfi_startproc
        lbu     t0, 0(s9)
        bne     t3, t0, .pos1_func_chk1
        lbu     t0, 2(s9)
        beq     t3, t0, .pos1_func_return

  .pos1_func_chk1:        
        lbu     t0, 4(s9)
        bne     t3, t0, .pos1_func_fail
        lbu     t0, 7(s9)
        beq     t3, t0, .pos1_func_return

  .pos1_func_fail:
        mv      t3, zero        

  .pos1_func_return:
        jr      ra
        .cfi_endproc

.type _pos2func, @function
_pos2func:
        .cfi_startproc
        lbu     t0, 0(s9)
        bne     t3, t0, .pos2_func_chk1
        lbu     t0, 1(s9)
        beq     t3, t0, .pos2_func_return

  .pos2_func_chk1:        
        lbu     t0, 5(s9)
        bne     t3, t0, .pos2_func_chk2
        lbu     t0, 8(s9)
        beq     t3, t0, .pos2_func_return

  .pos2_func_chk2:        
        lbu     t0, 4(s9)
        bne     t3, t0, .pos2_func_fail
        lbu     t0, 6(s9)
        beq     t3, t0, .pos2_func_return

  .pos2_func_fail:
        mv      t3, zero        

  .pos2_func_return:
        jr      ra
        .cfi_endproc

.type _pos3func, @function
_pos3func:
        .cfi_startproc
        lbu     t0, 0(s9)
        bne     t3, t0, .pos3_func_chk1
        lbu     t0, 6(s9)
        beq     t3, t0, .pos3_func_return

  .pos3_func_chk1:        
        lbu     t0, 4(s9)
        bne     t3, t0, .pos3_func_fail
        lbu     t0, 5(s9)
        beq     t3, t0, .pos3_func_return

  .pos3_func_fail:
        mv      t3, zero        

  .pos3_func_return:
        jr      ra
        .cfi_endproc

.type _pos4func, @function
_pos4func:
        .cfi_startproc
        lbu     t0, 0(s9)
        bne     t3, t0, .pos4_func_chk1
        lbu     t0, 8(s9)
        beq     t3, t0, .pos4_func_return

  .pos4_func_chk1:        
        lbu     t0, 2(s9)
        bne     t3, t0, .pos4_func_chk2
        lbu     t0, 6(s9)
        beq     t3, t0, .pos4_func_return

  .pos4_func_chk2:        
        lbu     t0, 3(s9)
        bne     t3, t0, .pos4_func_chk3
        lbu     t0, 5(s9)
        beq     t3, t0, .pos4_func_return

  .pos4_func_chk3:        
        lbu     t0, 1(s9)
        bne     t3, t0, .pos4_func_fail
        lbu     t0, 7(s9)
        beq     t3, t0, .pos4_func_return

  .pos4_func_fail:
        mv      t3, zero      

  .pos4_func_return:
        jr      ra
        .cfi_endproc

.type _pos5func, @function
_pos5func:
        .cfi_startproc
        lbu     t0, 2(s9)
        bne     t3, t0, .pos5_func_chk1
        lbu     t0, 8(s9)
        beq     t3, t0, .pos5_func_return

  .pos5_func_chk1:        
        lbu     t0, 3(s9)
        bne     t3, t0, .pos5_func_fail
        lbu     t0, 4(s9)
        beq     t3, t0, .pos5_func_return

  .pos5_func_fail:
        mv      t3, zero    

  .pos5_func_return:
        jr      ra
        .cfi_endproc

.type _pos6func, @function
_pos6func:
        .cfi_startproc
        lbu     t0, 0(s9)
        bne     t3, t0, .pos6_func_chk1
        lbu     t0, 3(s9)
        beq     t3, t0, .pos6_func_return

  .pos6_func_chk1:        
        lbu     t0, 2(s9)
        bne     t3, t0, .pos6_func_chk2
        lbu     t0, 4(s9)
        beq     t3, t0, .pos6_func_return

  .pos6_func_chk2:        
        lbu     t0, 7(s9)
        bne     t3, t0, .pos6_func_fail
        lbu     t0, 8(s9)
        beq     t3, t0, .pos6_func_return

  .pos6_func_fail:
        mv      t3, zero      

  .pos6_func_return:
        jr      ra
        .cfi_endproc

.type _pos7func, @function
_pos7func:
        .cfi_startproc
        lbu     t0, 1(s9)
        bne     t3, t0, .pos7_func_chk1
        lbu     t0, 4(s9)
        beq     t3, t0, .pos7_func_return

  .pos7_func_chk1:        
        lbu     t0, 6(s9)
        bne     t3, t0, .pos7_func_fail
        lbu     t0, 8(s9)
        beq     t3, t0, .pos7_func_return

  .pos7_func_fail:
        mv      t3, zero    

  .pos7_func_return:
        jr      ra
        .cfi_endproc

.type _pos8func, @function
_pos8func:
        .cfi_startproc
        lbu     t0, 2(s9)
        bne     t3, t0, .pos8_func_chk1
        lbu     t0, 5(s9)
        beq     t3, t0, .pos8_func_return

  .pos8_func_chk1:        
        lbu     t0, 0(s9)
        bne     t3, t0, .pos8_func_chk2
        lbu     t0, 4(s9)
        beq     t3, t0, .pos8_func_return

  .pos8_func_chk2:        
        lbu     t0, 6(s9)
        bne     t3, t0, .pos8_func_fail
        lbu     t0, 7(s9)
        beq     t3, t0, .pos8_func_return

  .pos8_func_fail:
        mv      t3, zero      

  .pos8_func_return:
        jr      ra
        .cfi_endproc

.endif
