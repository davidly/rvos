@echo off
setlocal

if "%1" == "" (set _runcmd=rvos -h:100) else (set _runcmd=%1 -h:100 )
if "%1" == "armos" (set _runcmd="..\armos\armos" -h:160 ..\armos\bin\rvos -h:100 )
if "%1" == "armoscl" (set _runcmd="..\armos\armoscl" -h:160 ..\armos\bin\rvoscl -h:100 )
if "%1" == "m68" (set _runcmd="..\m68\m68" -h:160 ..\m68\rvos\rvos -h:100 )
if "%1" == "sparcos" (set _runcmd="..\sparcos\sparcos" -h:160 ..\sparcos\bin\rvos-sparc.elf -h:100 )
if "%1" == "x64os" (set _runcmd="..\x64os\x64os" -h:160 ..\x64os\bin\rvos -h:100 )
if "%1" == "x32os" (set _runcmd="..\x64os\x32os" -h:160 ..\x64os\x32bin\rvos -h:100 )
if "%1" == "nested" (set _runcmd=rvos -h:160 linux\rvos -h:100 )

set outputfile=runall_test.txt
echo %date% %time% >%outputfile%

set _folderlist=bin0 bin1 bin2 bin3 binfast
set _applist=tcmp t e printint sieve simple tmuldiv tpi ts tarray tbits trw trw2 ^
             tmmap tstr tdir fileops ttime tm glob tap tsimplef tphi tf ttt td terrno ^
             t_setjmp tex mm tao pis ttypes nantst sleeptm tatomic lenum ^
             tregex trename nqueens

( for %%a in (%_applist%) do (
    echo %%a
    ( for %%f in (%_folderlist%) do (
        echo c_tests/%%f/%%a>>%outputfile%
        %_runcmd% c_tests\%%f\%%a >>%outputfile%
    ) )
) )

set _sapplist=tins sieve_rv e_rv tttu_rv
( for %%a in (%_sapplist%) do (
    echo %%a
    echo c_tests/%%a>>%outputfile%
    %_runcmd% c_tests\%%a >>%outputfile%
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

echo test ff . ff.c
set _folderlist=bin0 bin1 bin2 bin3 binfast
( for %%f in (%_folderlist%) do (
    echo test c_tests/%%f/ff . ff.c>>%outputfile%
    %_runcmd% c_tests\%%f\ff . ff.c>>%outputfile%
) )

echo test tgets
set _folderlist=bin0 bin1 bin2 bin3 binfast
( for %%f in (%_folderlist%) do (
    echo test c_tests/%%f/tgets>>%outputfile%
    %_runcmd% c_tests\%%f\tgets <c_tests\tgets.txt>>%outputfile%
) )

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


