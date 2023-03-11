#!/bin/bash

date_time=$(date)
echo "$date_time" >linux_test.txt

for arg in t tenv ts tf tap ttt sieve e tpi td tphi ttime;
do
    echo $arg >>linux_test.txt
    rvos $arg >>linux_test.txt
done

echo "an nap" >>linux_test.txt
rvos /h:30 an nap  >>linux_test.txt

echo "ba /p /q tp.bas" >>linux_test.txt
rvos /h:30 ba /p /q tp.bas >>linux_test.txt

echo "mysort /r words.txt sorted.txt" >>linux_test.txt
rvos /h:30 mysort /r words.txt sorted.txt >>linux_test.txt

echo "tcrash sph" >>linux_test.txt
rvos tcrash sph >>linux_test.txt

echo "tcrash mh" >>linux_test.txt
rvos tcrash mh >>linux_test.txt

diff baseline_linux_test.txt linux_test.txt