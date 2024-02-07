#!/bin/bash

date_time=$(date)
echo "$date_time" >linux_test.txt

for arg in t glob tenv ts tf tm tap ttt sieve e tpi tp1k tins td tphi ttime terrno fileops t_setjmp tex trw empty;
do
    echo $arg >>linux_test.txt
    rvos debianrv/$arg >>linux_test.txt
done

echo "tmmap test" >>linux_test.txt
rvos -h:0 -m:20 debianrv/tmmap >>linux_test.txt

echo "an nap" >>linux_test.txt
rvos -h:30 debianrv/an.elf nap  >>linux_test.txt

echo "ba -p -q tp.bas" >>linux_test.txt
rvos -h:30 debianrv/ba.elf -p -q tests/tp.bas >>linux_test.txt

echo "mysort /r words.txt sorted.txt" >>linux_test.txt
rvos -h:30 debianrv/mysort.elf /r tests/words.txt tests/sorted.txt >>linux_test.txt

echo "tcrash sph" >>linux_test.txt
rvos debianrv/tcrash sph >>linux_test.txt

echo "tcrash mh" >>linux_test.txt
rvos debianrv/tcrash mh >>linux_test.txt

for arg in ato e real tap ttt mysort fileops;
do
    echo $arg >>linux_test.txt
    rvos debianrv/rust/$arg >>linux_test.txt
done

diff baseline_linux_test.txt linux_test.txt
