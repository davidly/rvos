# BYTE magazine benchmark
#   #define SIZE 8190
#
#   char flags[SIZE+1];
#
#   int main()
#           {
#           int i,k;
#           int prime,count,iter;
#
#           for (iter = 1; iter <= 10; iter++) {    /* do program 10 times */
#                   count = 0;                      /* initialize prime counter */
#                   for (i = 0; i <= SIZE; i++)     /* set all flags TRUE */
#                           flags[i] = TRUE;
#                   for (i = 0; i <= SIZE; i++) {
#                           if (flags[i]) {         /* found a prime */
#                                   prime = i + i + 3;      /* twice index + 3 */
#                                   for (k = i + prime; k <= SIZE; k += prime)
#                                           flags[k] = FALSE;       /* kill all multiples */
#                                   count++;                /* primes found */
#                                   }
#                           }
#                   }
#           printf("%d primes.\n",count);           /*primes found in 10th pass */
#           return 0;
#           }
#
# k s0
# flags s1
# size s2
# prime s3
# count s4
# i s5
# iter s6
# & flags[ i ] s7

.set size, 8190
.set size_full, (size+2)

.data
  .align 8
  .done_string: .asciz "%u primes.\n"
  .align 8
  .flags: .zero size_full

  .align 8
  .print_buf: .zero 20
  .after_print_buf: .zero 1

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

        mv      t3, zero
        li      s6, 10

    _next_iteration:
        lla     s2, .flags
        li      t0, ( size_full - 8 )
        add     t2, s2, t0
        li      t1, 0x0101010101010101

      _init_next:
        sd      t1, (t2)
        addi    t2, t2, -8
        bge     t2, s2, _init_next

        mv      t1, zero
        lla     s1, .flags
        li      s2, size
        mv      s4, zero
        mv      s7, s1

      _outer:
        lbu     t0, (s7)                 # if flags[ i ]
        addi    s7, s7, 1
        beq     t0, zero, _outer

        sub     s5, s7, s1               # figure out i based on how far the pointer in flags progressed
        addi    s5, s5, -1
        bgt     s5, s2, _all_done

        addi    s3, s5, 3                # prime = i + 3
        add     s3, s3, s5               # prime += i
        add     s0, s3, s5               # k = prime + i
        bgt     s0, s2, _inc_count       # redundant check so _inner loop has just one conditional

      _inner:
        add     t0, s1, s0
        sb      zero, (t0)               # flags[ k ] = false
        add     s0, s0, s3               # k += prime
        ble     s0, s2, _inner           # k <= SIZE

      _inc_count:
        addi    s4, s4, 1
        j       _outer

      _all_done:
        addi    s6, s6, -1
        bne     s6, zero, _next_iteration

        lla     a0, .done_string
        mv      a1, s4
        jal     printf

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

