#riscv64-unknown-linux-gnu-c++ -fno-builtin -Ofast -I . -ggdb -mcmodel=medany -mabi=lp64d -march=rv64imad rvos.cxx riscv.cxx -o rvos.elf  -static --entry _start

riscv64-unknown-linux-gnu-c++ -D RVOS -fno-builtin -I . -ggdb -O3 -mcmodel=medany -mabi=lp64d -march=rv64imadc rvos.cxx riscv.cxx -o rvos.elf -static