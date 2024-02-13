#!/bin/bash

# usage: rt.sh                          --- run tests in tests folder
#        rt.sh debianrv                 --- run tests in debianrv
#        rt.sh debianrv debianrv\rvos   --- run tests in debianrv using nested debianrv\rvos
#        rt.sh tests rvos.elf           --- run tests in tests using nested rvos.elf

_outputfile="linux_test.txt"

_testfolder="tests"
if [ "$1" != "" ]; then
	_testfolder=$1
fi

_rvoscmd="rvos"
if [ "$2" != "" ]; then
	_rvoscmd="rvos -h:60 $2"
fi

echo $(date) >$_outputfile

for arg in t glob tenv ts tf tm tap ttt sieve e tpi tp tins td tphi ttime terrno fileops t_setjmp tex trw empty;
do
    echo running C test app $arg using brk heap >>$_outputfile
    $_rvoscmd -h:10 -m:0 $_testfolder/$arg >>$_outputfile

    echo running C test app $arg using mmap heap >>$_outputfile
    $_rvoscmd -h:0 -m:10 $_testfolder/$arg >>$_outputfile
done

for arg in ato real ttt e tap tphi mysort fileops;
do
    echo running rust test app $arg using brk heap >>$_outputfile
    $_rvoscmd -h:10 -m:0 debianrv/rust/$arg >>$_outputfile

    echo running rust test app $arg using mmap heap >>$_outputfile
    $_rvoscmd -h:0 -m:10 debianrv/rust/$arg >>$_outputfile
done

for arg in ml mh pcl pch spl sph spm
do
    echo tcrash $arg >>$_outputfile
    $_rvoscmd $_testfolder/tcrash $arg >>$_outputfile
done

echo "tmmap test" >>$_outputfile
$_rvoscmd -h:0 -m:20 $_testfolder/tmmap >>$_outputfile

echo "an nap" >>$_outputfile
$_rvoscmd -h:30 $_testfolder/an nap  >>$_outputfile

echo "ba /p /q tp.bas" >>$_outputfile
$_rvoscmd -h:30 $_testfolder/ba /p /q $_testfolder/tp.bas >>$_outputfile

echo "mysort /r words.txt sorted.txt" >>$_outputfile
$_rvoscmd -h:30 $_testfolder/mysort /r $_testfolder/words.txt $_testfolder/sorted.txt >>$_outputfile

echo "running ttty tests" >>$_outputfile
echo "blahblah" >bar
$_rvoscmd $_testfolder/ttty >>$_outputfile
$_rvoscmd $_testfolder/ttty <bar >>$_outputfile
echo "should see no, yes, yes, yes"
$_rvoscmd $_testfolder/ttty <bar 
$_rvoscmd $_testfolder/ttty

echo $(date) >>$_outputfile
diff baseline_$_outputfile $_outputfile
