#!/bin/bash

for arg in t tenv ts tf tap ttt sieve e an tdir tpi td ba mysort tphi ttime;
do
    echo $arg
    mt.sh "$arg"
done

echo "mta"
mta.sh tins

echo "tcrash"
mt_shell.sh "tcrash"