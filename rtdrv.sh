#!/bin/bash

date_time=$(date)
echo "$date_time" >linuxdrv_test.txt

for arg in t glob tenv ts tf tm tap ttt sieve e tpi tp1k tins td tphi ttime terrno fileops t_setjmp tex trw empty;
do
    echo $arg >>linuxdrv_test.txt
    rvos debianrv/$arg >>linuxdrv_test.txt
done

echo "tmmap test" >>linuxdrv_test.txt
rvos -h:0 -m:20 debianrv/tmmap >>linuxdrv_test.txt

echo "an nap" >>linuxdrv_test.txt
rvos -h:30 debianrv/an.elf nap  >>linuxdrv_test.txt

echo "ba -p -q tp.bas" >>linuxdrv_test.txt
rvos -h:30 debianrv/ba.elf -p -q tests/tp.bas >>linuxdrv_test.txt

echo "mysort /r words.txt sorted.txt" >>linuxdrv_test.txt
rvos -h:30 debianrv/mysort.elf /r tests/words.txt tests/sorted.txt >>linuxdrv_test.txt

echo "tcrash sph" >>linuxdrv_test.txt
rvos debianrv/tcrash sph >>linuxdrv_test.txt

echo "tcrash mh" >>linuxdrv_test.txt
rvos debianrv/tcrash mh >>linuxdrv_test.txt

for arg in ato e real tap ttt mysort fileops;
do
    echo $arg >>linuxdrv_test.txt
    rvos debianrv/rust/$arg >>linuxdrv_test.txt
done

diff baseline_linuxdrv_test.txt linuxdrv_test.txt
