#!/bin/bash


for arg in ato e fileops mysort real tap td tphi ttt tmm;
do
    echo $arg

    for optflag in 0 1 2 3;
    do
        mkdir bin"$optflag" 2>/dev/null
        rustc --edition 2021 --out-dir bin"$optflag" -C opt-level="$optflag" -C target-feature=+crt-static "$arg".rs
    done    
done

