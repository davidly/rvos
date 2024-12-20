#!/bin/bash

for arg in tcmp t e printint sieve simple tmuldiv tpi ts tarray tbits trw tmmap tstr \
           fileops ttime tm glob tap tsimplef tphi tf ttt td terrno t_setjmp tex \
           pis mm sleeptm tatomic lenum tregex an ba;
do
    echo $arg
    for optflag in 0 1 2 3 fast;
    do
        mkdir bin"$optflag" 2>/dev/null
        riscv64-unknown-linux-gnu-c++ "$arg".c -o bin"$optflag"/"$arg" -O"$optflag" -mcmodel=medany -mabi=lp64d -march=rv64imadc -latomic -static -fsigned-char -Wno-format -Wno-format-security
    done
done

