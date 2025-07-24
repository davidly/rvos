#!/bin/bash

/opt/riscv/bin/riscv64-unknown-elf-c++ $1.s -o $1 -mcmodel=medany -mabi=lp64d -march=rv64imadcv -latomic -static
