#!/bin/bash

for optflag in 0 1 2 3 fast;
do
    mkdir bin"$optflag" 2>/dev/null
    _gnubuild="g++ $1.c -o bin"$optflag"/$1 -O"$optflag" -static -fsigned-char -Wno-format -Wno-format-security"

    if [ "$optflag" != "fast" ]; then
        $_gnubuild &
    else    
        $_gnubuild
    fi
done

