#!/bin/bash

for arg in tcmp t e printint sieve simple tmuldiv tpi ts tarray tbits trw trw2 tmmap tstr \
           tdir fileops ttime tm glob tap tsimplef tphi tf ttt td terrno t_setjmp tex \
           tprintf pis mm tao ttypes nantst sleeptm tatomic lenum tregex trename \
           nqueens ff an ba tgets;
do
    echo $arg
    for optflag in 0 1 2 3 fast;
    do
        mkdir bin"$optflag" 2>/dev/null
        _gnubuild="riscv64-unknown-linux-gnu-c++ "$arg".c -o bin"$optflag"/"$arg" -O"$optflag" -mcmodel=medany -mabi=lp64d -march=rv64imadc -latomic -static -fsigned-char -Wno-format -Wno-format-security"

        if [ "$optflag" != "fast" ]; then
            $_gnubuild &
        else    
            $_gnubuild
        fi
    done
done

# build assembly tests

riscv64-unknown-linux-gnu-c++ tins.s -o tins -mcmodel=medany -mabi=lp64d -march=rv64imadcv -latomic -static
riscv64-unknown-linux-gnu-c++ tttu_rv.s -o tttu_rv -mcmodel=medany -mabi=lp64d -march=rv64imadcv -latomic -static
riscv64-unknown-linux-gnu-c++ e_rv.s -o e_rv -mcmodel=medany -mabi=lp64d -march=rv64imadcv -latomic -static
riscv64-unknown-linux-gnu-c++ sieve_rv.s -o sieve_rv -mcmodel=medany -mabi=lp64d -march=rv64imadcv -latomic -static

echo "Waiting for all processes to complete..."
wait

