#!/bin/bash

_rvoscmd="rvos"
if [ "$1" = "nested" ]; then
    _rvoscmd="rvos -h:200 bin/rvos"
elif [ "$1" = "native" ]; then
    _rvoscmd=""
elif [ "$1" = "rvoscl" ]; then
    _rvoscmd="rvoscl -h:200"
elif [ "$1" = "armos" ]; then
    _rvoscmd="../ArmOS/armos -h:200 ../ArmOS/bin/rvos -h:100"
elif [ "$1" = "x64os" ]; then
    _rvoscmd="../x64os/x64os -h:200 ../x64os/bin/rvos -h:100"
elif [ "$1" = "armoscl" ]; then
    _rvoscmd="../ArmOS/armoscl -h:200 ../ArmOS/bin/rvos -h:100"
elif [ "$1" = "m68" ]; then
    _rvoscmd="../m68/m68 -h:200 ../m68/rvos/rvos -h:100"
fi    

outputfile="runall_test.txt"
date_time=$(date)
echo "$date_time" >$outputfile

for arg in tcmp t e printint sieve simple tmuldiv tpi ts tarray tbits trw trw2 \
           tmmap tstr tdir fileops ttime tm glob tap tsimplef tphi tf ttt td terrno \
           t_setjmp tex mm tao pis ttypes nantst sleeptm tatomic lenum \
           tregex trename nqueens;
do
    echo $arg
    for opt in 0 1 2 3 fast;
    do
        echo c_tests/bin$opt/$arg >>$outputfile
        $_rvoscmd c_tests/bin$opt/$arg >>$outputfile
    done
done

for arg in tins sieve_rv e_rv tttu_rv
do
    echo $arg
    echo c_tests/$arg >>$outputfile
    $_rvoscmd c_tests/$arg >>$outputfile
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

echo test ff . ff.c
for opt in 0 1 2 3 fast;
do
    echo test c_tests/bin$opt/ff . ff.c >>$outputfile
    $_rvoscmd c_tests/bin$opt/ff . ff.c >>$outputfile
done

echo test tgets
for optflag in 0 1 2 3 fast;
do
    echo test c_tests/bin$optflag/tgets >>$outputfile
    $_rvoscmd c_tests/bin$optflag/tgets <c_tests/tgets.txt >>$outputfile
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
