# compute the first 192 digits of e
# replicates this C code:
#    #define DIGITS_TO_FIND 200 /*9009*/
#    int main() {
#      int N = DIGITS_TO_FIND;
#      int x = 0;
#      int a[ DIGITS_TO_FIND ];
#      int n;
#
#      for (n = N - 1; n > 0; --n)
#          a[n] = 1;
#
#      a[1] = 2, a[0] = 0;
#      while (N > 9) {
#          n = N--;
#          while (--n) {
#              a[n] = x % n;
#              x = 10 * a[n-1] + x/n;
#          }
#          printf("%d", x);
#      }
#  
#      printf( "\ndone\n" );
#      return 0;
#    }
#
# & a[n] is in s1
# array is in s2
# N is in s3
# x is in s4
# n is in s5
# 10 is in s10
# 9 is in s9

.set array_size, 200

.data
  .p2align 4
  .array: .zero 2*array_size

  .p2align 4
  .done_string: .asciz "\ndone"
  .print_buffer: .space 24

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

        lla     s2, .array
        li      t0, ( ( array_size * 2 ) - 8 )
        add     t2, s2, t0
        li      t1, 0x0001000100010001

      _init_next:
        sd      t1, (t2)
        addi    t2, t2, -8
        bne     t2, s2, _init_next

        li      t1, 0x0001000100020000
        sd      t1, (s2)

        li      s9, 9
        li      s10, 10
        li      s3, array_size
        mv      s4, zero

      _outer:
        beq     s3, s9, _loop_done
        addi    s3, s3, -1
        mv      s5, s3
        mv      s1, s2
        add     s1, s1, s5
        add     s1, s1, s5

      _inner:
        remu    t1, s4, s5
        sh      t1, (s1)
        add     s1, s1, -2
        lhu     t0, (s1)
        mul     t0, t0, s10
        divu    t1, s4, s5
        add     s4, t0, t1
        addi    s5, s5, -1
        bne     s5, zero, _inner

      _print_digit:
         mv      a0, s4
        call     print_unsigned_long
        j       _outer

      _loop_done:
        lla     a0, .done_string
        jal     puts

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

.globl print_unsigned_long
print_unsigned_long:
    # Save necessary registers.
    # No need to save s1 or s2 as they are not used in this version.
    addi sp, sp, -8
    sd ra, 0(sp)

    mv a1, a0          # a1 = number to print
    li a2, 10          # a2 = divisor (10)
    lla a3, .print_buffer + 23 # a3 = pointer to end of buffer
                             # (24 bytes - 1 for zero-based indexing)

    # Handle the special case for number 0
    beqz a1, .L_print_zero

.L_loop_divide:
    remu a0, a1, a2    # a0 = a1 % 10 (unsigned remainder)
    addi a0, a0, '0'   # a0 = digit_char
    sb a0, 0(a3)       # Store the character in the buffer
    addi a3, a3, -1    # Move buffer pointer backwards
    divu a1, a1, a2    # a1 = a1 / 10 (unsigned quotient)
    bnez a1, .L_loop_divide # Continue until quotient is zero

.L_print:
    # After the loop, a3 points to the start of the string.
    # Now make the write syscall.
    li a0, 1             # a0 = file descriptor (1 for stdout)
    addi a1, a3, 1       # a1 = buffer address (start of string)
    lla a2, .print_buffer + 24 # a2 = end of buffer address
    sub a2, a2, a1       # a2 = length of string
    li a7, 64            # a7 = syscall number for write
    ecall                # Execute the write syscall

    j .L_done

.L_print_zero:
    # If the number is 0, just print a single '0'
    lla a1, .print_buffer + 23 # a1 = buffer address
    li a0, '0'
    sb a0, 0(a1)

    li a0, 1             # fd=1
    mv a1, a1            # buffer ptr
    li a2, 1             # length=1
    li a7, 64            # write syscall
    ecall
    
.L_done:
    # Restore registers and return.
    ld ra, 0(sp)
    addi sp, sp, 8
    ret
