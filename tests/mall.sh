#!/bin/bash

for arg in t tenv ts tf tap ttt sieve e an tpi td ba mysort tphi ttime;
do
    echo $arg
    mt.sh "$arg"
done

echo "tcrash"
mt_shell.sh "tcrash"