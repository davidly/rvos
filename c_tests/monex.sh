#!/bin/bash

for optflag in 0 1 2 3 fast;
do
    mkdir bin"$optflag" 2>/dev/null
    _gnubuild="riscv64-unknown-linux-gnu-c++ $1.c -o bin"$optflag"/$1 -O"$optflag" -mcmodel=medany -mabi=lp64d -march=rv64imadcv -latomic -static -fsigned-char -Wno-format -Wno-format-security"

    if [ "$optflag" != "fast" ]; then
        $_gnubuild &
    else    
        $_gnubuild
    fi
done

echo "Waiting for all processes to complete..."
wait
