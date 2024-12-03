#!/bin/bash

outputfile="linuxdrv_test.txt"
date_time=$(date)
echo "$date_time" >$outputfile

for arg in t glob tenv ts tf tm tap ttt sieve e tpi tp1k tins td tphi ttime terrno fileops t_setjmp tex trw empty;
do
    echo $arg >>$outputfile
    rvos debianrv/$arg >>$outputfile
done

for arg in ato e real tap ttt mysort fileops;
do
    echo $arg >>$outputfile
    rvos debianrv/rust/$arg >>$outputfile
done

echo "tmmap test" >>$outputfile
rvos -h:0 -m:20 debianrv/tmmap >>$outputfile

echo "an nap" >>$outputfile
rvos -h:30 debianrv/an.elf nap  >>$outputfile

echo "ba -p -q tp.bas" >>$outputfile
rvos -h:30 debianrv/ba.elf -p -q tests/tp.bas >>$outputfile

echo "mysort /r words.txt sorted.txt" >>$outputfile
rvos -h:30 debianrv/mysort.elf /r tests/words.txt tests/sorted.txt >>$outputfile

for arg in ml mh pcl pch spl sph spm
do
    echo tcrash $arg >>$outputfile
    rvos debianrv/tcrash $arg >>$outputfile
done

echo "running ttty tests" >>$outputfile
echo "blahblah" >bar
rvos debianrv/ttty >>$outputfile
rvos debianrv/ttty <bar >>$outputfile
echo "should see no, yes, yes, yes"
rvos debianrv/ttty <bar 
rvos debianrv/ttty

echo "$date_time" >>$outputfile
diff baseline_$outputfile $outputfile
