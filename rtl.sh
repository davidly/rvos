#!/bin/bash

date_time=$(date)
echo "$date_time" >linux_test.txt

for arg in t tenv ts tf tap ttt sieve e tpi tp1k tins td tphi ttime terrno;
do
    echo $arg >>linux_test.txt
    rvos linux/$arg >>linux_test.txt
done

echo "an nap" >>linux_test.txt
rvos /h:30 linux/an nap  >>linux_test.txt

echo "ba /p /q tp.bas" >>linux_test.txt
rvos /h:30 linux/ba /p /q tests/tp.bas >>linux_test.txt

echo "mysort /r words.txt sorted.txt" >>linux_test.txt
rvos /h:30 linux/mysort /r tests/words.txt tests/sorted.txt >>linux_test.txt

echo "tcrash sph" >>linux_test.txt
rvos linux/tcrash sph >>linux_test.txt

echo "tcrash mh" >>linux_test.txt
rvos linux/tcrash mh >>linux_test.txt

diff baseline_linux_test.txt linux_test.txt
