#g++ -fno-builtin -I . rvos.cxx riscv.cxx -o rvos --entry _start

riscv64-unknown-linux-gnu-c++ -fno-builtin -I . -ggdb -mcmodel=medany -mabi=lp64d -march=rv64imad -I . rvos.cxx riscv.cxx -o rvos.elf  -static --entry _start