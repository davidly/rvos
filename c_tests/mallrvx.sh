#!/bin/bash

includes="-I../gnu11rv-arm/include -I../gnu11rv-arm/include/c++/11 -I../gnu11rv-arm/include/c++/11/riscv64-linux-gnu"

for arg in tcmp t e printint sieve simple tmuldiv tpi ts tarray tbits trw trw2 tmmap tstr \
           tdir fileops ttime tm glob tap tsimplef tphi tf ttt td terrno t_setjmp tex \
           tprintf pis mm tao ttypes nantst sleeptm tatomic lenum tregex trename \
           nqueens ff an ba;
do
    echo $arg
    for optflag in 0 1 2 3 fast;
    do
        mkdir bin"$optflag" 2>/dev/null
        _gnubuild="/usr/riscv64/riscv64-linux-gnu-g++-11 -std=c++11 $arg.c -o bin"$optflag"/$arg -O"$optflag" $includes -latomic -static -fsigned-char -Wno-format -Wno-format-security"

        if [ "$optflag" != "fast" ]; then
            $_gnubuild &
        else    
            $_gnubuild
        fi
    done
done

# build assembly tests

/usr/riscv64/riscv64-linux-gnu-g++-11 tins.s -o tins -mcmodel=medany -mabi=lp64d -march=rv64imadcv -latomic -static

echo "Waiting for all processes to complete..."
wait
