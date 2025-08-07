#!/bin/bash

for optflag in 0 1 2 3 fast;
do
    mkdir bin"$optflag" 2>/dev/null
#    _gnubuild="/opt/riscv/bin/riscv64-unknown-elf-c++ -std=c++11 $1.c -o bin"$optflag"/$1 -O"$optflag" -static -fsigned-char -Wno-format -Wno-format-security"
#    _gnubuild="riscv64-unknown-linux-gnu-c++ -std=c++11 $1.c -o bin"$optflag"/$1 -O"$optflag" -static -fsigned-char -Wno-format -Wno-format-security"
    _gnubuild="/usr/bin/riscv64-linux-gnu-g++ -std=c++11 $1.c -o bin"$optflag"/$1 -O"$optflag" -latomic -static -fsigned-char -Wno-format -Wno-format-security"
    

    if [ "$optflag" != "fast" ]; then
        $_gnubuild &
    else    
        $_gnubuild
    fi
done

echo "Waiting for all processes to complete..."
wait
