riscv64-unknown-linux-gnu-c++ $1.s -o $1 -mcmodel=medany -mabi=lp64d -march=rv64imadcv -latomic -static
