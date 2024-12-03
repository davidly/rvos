riscv64-unknown-linux-gnu-c++ -ggdb -mcmodel=medany -mabi=lp64d -march=rv64imad -I .. -I . $1.s -o $1.elf -static
