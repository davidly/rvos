@echo off
setlocal

rem if "%1" == "" (set _runcmd=rvos -h:100) else (set _runcmd=rvos -h:160 bin\rvos -h:100 )
rem if "%1" == "" (set _runcmd=rvos -h:100) else (set _runcmd=..\armos\armos -h:160 ..\armos\bin\rvos -h:100 )
if "%1" == "" (set _runcmd=rvos -h:100) else (set _runcmd=..\armos\armos -h:160 ..\armos\bin\rvoscl -h:100 )

set outputfile=runall_windows_test.txt
echo %date% %time% >%outputfile%

set _folderlist=bin0 bin1 bin2 bin3 binfast
set _applist=tcmp t e printint sieve simple tmuldiv tpi ts ttt tarray tbits trw ^
             tmmap tstr fileops ttime tm glob tap tsimplef tf td terrno ^
             t_setjmp tex mm tao pis ttypes nantst sleeptm tatomic lenum tregex
set _optlist=6 8 a d 3 i I m o r x

( for %%f in (%_folderlist%) do ( call :folderRun %%f ) )

set _rustfolders=bin0 bin1 bin2 bin3

( for %%f in (%_rustfolders%) do ( call :rustfolder %%f ) )

echo test ntvao
echo test ntvao >>%outputfile%
%_runcmd% bin\ntvao -c ttt1.hex >>%outputfile%

echo test ntvcm
echo test ntvcm >>%outputfile%
%_runcmd% bin\ntvcm tttcpm.com >>%outputfile%

echo test ntvdm
echo test ntvdm >>%outputfile%
%_runcmd% bin\ntvdm tttmasm.exe 1 >>%outputfile%

echo test rvos
echo test rvos >>%outputfile%
%_runcmd% bin\rvos ttt.elf 1 >>%outputfile%

echo test armos
echo test armos >>%outputfile%
%_runcmd% bin\armos ttt_arm64 1 >>%outputfile%

echo %date% %time% >>%outputfile%
diff baseline_%outputfile% %outputfile%

goto :allDone

:folderRun

( for %%a in (%_applist%) do ( call :appRun c_tests\%%f\%%a ) )

echo test c_tests\%~1\an
echo test c_tests\%~1\an >>%outputfile%
call :anTest c_tests\%~1

echo test c_tests\%~1\ba
echo test c_tests\%~1\ba >>%outputfile%
call :baTest c_tests\%~1

exit /b 0

:appRun
echo test %~1
echo test %~1 >>%outputfile%
%_runcmd% %~1 >>%outputfile%
exit /b /o

:rustRun
echo test %~1
echo test %~1 >>%outputfile%
%_runcmd% %~1 >>%outputfile%
exit /b /o

:rustfolder
set _rustlist=e ttt fileops ato tap real tphi mysort
( for %%a in (%_rustlist%) do ( call :rustRun rust_tests\%%f\%%a ) )
exit /b /o

:anTest
%_runcmd% %~1\an david lee >>%outputfile%
exit /b /o

:baTest
%_runcmd% %~1\ba tp.bas >>%outputfile%
( for %%o in (%_optlist%) do ( %_runcmd% %~1\ba -a:%%o -x tp.bas >>%outputfile% ) )
exit /b /o

:allDone


