#!/bin/bash

date_time=$(date)
echo "$date_time" >linux_test.txt

for arg in t glob tenv ts tf tm tap ttt sieve e tpi tp1k tins td tphi ttime terrno fileops t_setjmp tex trw empty;
do
    echo $arg >>linux_test.txt
    rvos tests/$arg >>linux_test.txt
done

for arg in ato real ttt e tap tphi mysort fileops;
do
    echo $arg >>linux_test.txt
    rvos debianrv/rust/$arg >>linux_test.txt
done

echo "an nap" >>linux_test.txt
rvos -h:30 tests/an nap  >>linux_test.txt

echo "ba /p /q tp.bas" >>linux_test.txt
rvos -h:30 tests/ba /p /q tests/tp.bas >>linux_test.txt

echo "mysort /r words.txt sorted.txt" >>linux_test.txt
rvos -h:30 tests/mysort /r tests/words.txt tests/sorted.txt >>linux_test.txt

echo "tcrash sph" >>linux_test.txt
rvos tests/tcrash sph >>linux_test.txt

echo "tcrash mh" >>linux_test.txt
rvos tests/tcrash mh >>linux_test.txt

echo "running ttty tests" >>linux_test.txt
echo "blahblah" >bar
rvos tests/ttty >>linux_test.txt
rvos tests/ttty <bar >>linux_test.txt
echo "should see no, yes, yes, yes"
rvos tests/ttty <bar 
rvos tests/ttty

diff baseline_linux_test.txt linux_test.txt
