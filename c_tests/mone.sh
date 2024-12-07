#!/bin/bash

for optflag in 0 1 2 3 fast;
do
    mkdir bin"$optflag" 2>/dev/null
    g++ $1.c -o bin"$optflag"/$1 -O"$optflag" -static -fsigned-char
done

