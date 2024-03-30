#!/bin/bash

for arg in t glob tenv ts tf tm tap ttt sieve e an tdir tpi td ba mysort tphi ttime terrno fileops empty t_setjmp tex trw ttty tmmap mid tmuldiv;
do
    echo $arg
    mt.sh "$arg"
done

echo "mta"
mta.sh tins

echo "tcrash"
mt_shell.sh "tcrash"

../rvos ba /a:r tp1k.bas /x /q
./ma.sh tp1k

../rvos ba /a:r tp.bas /x /q
./ma.sh tp
