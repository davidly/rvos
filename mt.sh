riscv64-unknown-linux-gnu-as -a=rvos_shell.lst -mabi=lp64d -march=rv64imad -fpic rvos_shell.s -o rvos_shell.o

riscv64-unknown-linux-gnu-c++ -ggdb -mcmodel=medany -mabi=lp64d -march=rv64imad -I . $1.c -o $1.elf rvos_shell.o -static
