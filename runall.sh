#!/bin/bash

_rvoscmd="rvos"
if [ "$1" != "" ]; then
    _rvoscmd="rvos -h:200 bin/rvos"
fi    

outputfile="runall_linux_test.txt"
date_time=$(date)
echo "$date_time" >$outputfile

for arg in tcmp t e printint sieve simple tmuldiv tpi ts tarray tbits trw tmmap tstr \
           fileops ttime tm glob tap tsimplef tphi tf ttt td terrno t_setjmp tex \
           mm tao pis ttypes nantst sleeptm tatomic lenum tregex;
do
    echo $arg
    for opt in 0 1 2 3 fast;
    do
        echo c_tests/bin$opt/$arg >>$outputfile
        $_rvoscmd c_tests/bin$opt/$arg >>$outputfile
    done
done

echo test AN
for opt in 0 1 2 3 fast;
do
    echo c_tests/bin$opt/an david lee >>$outputfile
    $_rvoscmd c_tests/bin$opt/an david lee >>$outputfile
done

echo test BA
for opt in 0 1 2 3 fast;
do
    echo c_tests/bin$opt/ba c_tests/tp.bas >>$outputfile
    $_rvoscmd c_tests/bin$opt/ba c_tests/tp.bas >>$outputfile
    for codegen in 6 8 a d 3 i I m o r x;
    do
        $_rvoscmd c_tests/bin$opt/ba -a:$codegen -x c_tests/tp.bas >>$outputfile
    done
done

for arg in e ttt fileops ato tap real tphi mysort tmm;
do
    echo $arg
    for opt in 0 1 2 3;
    do
        echo rust_tests/bin$opt/$arg >>$outputfile
        $_rvoscmd rust_tests/bin$opt/$arg >>$outputfile
    done
done

date_time=$(date)
echo "$date_time" >>$outputfile
diff baseline_$outputfile $outputfile
