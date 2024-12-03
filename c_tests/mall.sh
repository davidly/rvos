#!/bin/bash

for arg in tcmp e printint sieve simple tmuldiv tpi ts tarray tbits trw tmmap tstr \
           fileops ttime tm glob tap tsimplef tphi tf ttt td terrno t_setjmp tex \
           pis mm sleeptm tatomic an ba;
do
    echo $arg
    for optflag in 0 1 2 3 fast;
    do
        mkdir bin"$optflag" 2>/dev/null
        g++ "$arg".c -o bin"$optflag"/"$arg" -O"$optflag" -static -fsigned-char
    done
done

