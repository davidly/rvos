@echo off
setlocal

if "%1" == "" (set _runcmd=rvos -h:100) else (set _runcmd=%1 -h:100 )
if "%1" == "armos" (set _runcmd="..\armos\armos" -h:160 ..\armos\bin\rvos -h:100 )
if "%1" == "armoscl" (set _runcmd="..\armos\armoscl" -h:160 ..\armos\bin\rvoscl -h:100 )
if "%1" == "m68" (set _runcmd="..\m68\m68" -h:160 ..\m68\rvos\rvos -h:100 )
if "%1" == "nested" (set _runcmd=rvos -h:160 bin\rvos -h:100 )

set outputfile=runall_test.txt
echo %date% %time% >%outputfile%

set _folderlist=bin0 bin1 bin2 bin3 binfast
set _applist=tcmp t e printint sieve simple tmuldiv tpi ts tarray tbits trw ^
             tmmap tstr tdir fileops ttime tm glob tap tsimplef tphi tf ttt td terrno ^
             t_setjmp tex mm tao pis ttypes nantst sleeptm tatomic lenum ^
             tregex trename

( for %%a in (%_applist%) do (
    echo %%a
    ( for %%f in (%_folderlist%) do (
        echo c_tests/%%f/%%a>>%outputfile%
        %_runcmd% c_tests\%%f\%%a >>%outputfile%
    ) )
) )

echo test AN
( for %%f in (%_folderlist%) do (
    echo c_tests/%%f/an david lee>>%outputfile%
    %_runcmd% c_tests\%%f\an david lee >>%outputfile%
) )

echo test BA
set _optlist=6 8 a d 3 i I m o r x

( for %%f in (%_folderlist%) do (
    echo c_tests/%%f/ba c_tests/tp.bas>>%outputfile%
    %_runcmd% c_tests\\%%f\ba c_tests\tp.bas >>%outputfile%
    ( for %%o in (%_optlist%) do (
        %_runcmd% c_tests\%%f\ba -a:%%o -x c_tests\tp.bas >>%outputfile%
    ) )
) )

echo test TINS
echo c_tests/tins>>%outputfile%
%_runcmd% c_tests\tins >>%outputfile%

set _rustlist=e ttt fileops ato tap real tphi mysort tmm
set _rustfolders=bin0 bin1 bin2 bin3

( for %%a in (%_rustlist%) do (
    echo %%%a
    ( for %%f in (%_rustfolders%) do (
        echo rust_tests/%%f/%%a>>%outputfile%
        %_runcmd% rust_tests\%%f\%%a >>%outputfile%
    ) )
) )

echo %date% %time% >>%outputfile%
dos2unix %outputfile%
diff baseline_%outputfile% %outputfile%


