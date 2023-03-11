#riscv64-unknown-linux-gnu-c++ -fno-builtin -Ofast -I . -ggdb -mcmodel=medany -mabi=lp64d -march=rv64imad rvos.cxx riscv.cxx -o rvos.elf  -static --entry _start

riscv64-unknown-linux-gnu-c++ -Ofast -D NDEBUG -fno-builtin -I . -ggdb -mcmodel=medany -mabi=lp64d -march=rv64imadc rvos.cxx riscv.cxx -o rvos.elf -static