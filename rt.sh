#!/bin/bash

date_time=$(date)
echo "$date_time" >linux_test.txt

for arg in t tenv ts tf tm tap ttt sieve e tpi tp1k tins td tphi ttime tdir;
do
    echo $arg >>linux_test.txt
    rvos tests/$arg >>linux_test.txt
done

echo "an nap" >>linux_test.txt
rvos /h:30 tests/an nap  >>linux_test.txt

echo "ba /p /q tp.bas" >>linux_test.txt
rvos /h:30 tests/ba /p /q tests/tp.bas >>linux_test.txt

echo "mysort /r words.txt sorted.txt" >>linux_test.txt
rvos /h:30 tests/mysort /r tests/words.txt tests/sorted.txt >>linux_test.txt

echo "tcrash sph" >>linux_test.txt
rvos tests/tcrash sph >>linux_test.txt

echo "tcrash mh" >>linux_test.txt
rvos tests/tcrash mh >>linux_test.txt

diff baseline_linux_test.txt linux_test.txt
