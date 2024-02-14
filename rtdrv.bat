@echo off
setlocal

rem any command-line argument means run rvos nested in rvos
if "%1" == "" (set _rvoscmd=rvos) else (set _rvoscmd=rvos -h:60 debianrv\rvos)

set outputfile=testdrv_rvos.txt
echo %date% %time% >"%outputfile%"

robocopy \\lee-server\documents\scratch\debianrv debianrv * /s >>%outputfile%

set _testlist=t glob ts sieve e tap tpi tphi tins terrno tp ttt tf^
              tm ttime td fileops t_setjmp tex trw empty

( for %%t in (%_testlist%) do ( call :testRun %%t ) )

goto :runRustTests

:testRun

echo running C test app %~1 using brk heap >>"%outputfile%"
rem using brk heap, so old gcc's libc is ok
%_rvoscmd% -h:10 -m:0 debianrv\%~1 >>"%outputfile%"

echo running C test app %~1 using mmap heap >>"%outputfile%"
rem using mmap heap, so a newer libc is required to not require brk
%_rvoscmd% -h:0 -m:10 debianrv\%~1 >>"%outputfile%"

exit /b 0

:runRustTests

set _testlist=ato real ttt e tap tphi mysort fileops

( for %%t in (%_testlist%) do ( call :testRunRust %%t ) )

:crashTests

set _testlist=ml mh pcl pch spl sph spm

( for %%t in (%_testlist%) do ( call :testRunCrash %%t ) )

goto :runSingletonTests

:testrunCrash

echo running tcrash app %~1 >>%outputfile%
%_rvoscmd% debianrv\tcrash %~1 >>%outputfile%

exit /b 0

:testRunRust

echo running rust test app %~1 using brk heap >>"%outputfile%"
%_rvoscmd% -h:10 -m:0 debianrv\rust\%~1 >>"%outputfile%"

echo running rust test app %~1 using mmap >>"%outputfile%"
%_rvoscmd% -h:0 -m:10 debianrv\rust\%~1 >>"%outputfile%"

exit /b 0

:runSingletonTests

echo running tmmap >>%outputfile%
%_rvoscmd% -h:0 -m:20 debianrv\tmmap >>%outputfile%

echo running ttty tests >>%outputfile%
echo blahblah >bar
%_rvoscmd% debianrv\ttty >>%outputfile%
%_rvoscmd% debianrv\ttty <bar >>%outputfile%
echo should see no, yes, yes, yes
%_rvoscmd% debianrv\ttty <bar 
%_rvoscmd% debianrv\ttty

%_rvoscmd% debianrv\ba debianrv\tp.bas >>%outputfile%

%_rvoscmd% -h:2 debianrv\mysort /q /u debianrv\words.txt debianrv\sorted.txt >>%outputfile%
%_rvoscmd% -h:40 debianrv\an david lee >>%outputfile%

%_rvoscmd% -h:30 debianrv\rvos debianrv\tp.elf 1 >>%outputfile%
%_rvoscmd% -h:30 debianrv\ntvao -c ../ntvao/ttt1.hex >>%outputfile%
%_rvoscmd% -h:30 debianrv\ntvcm -c ../ntvcm/tttcpm.com 1 >>%outputfile%
%_rvoscmd% -h:30 debianrv\ntvdm -c ../ntvdm/ttt8086.com 1 >>%outputfile%

echo running ntvao, ntvcm, and ntvdm >>%outputfile%
%_rvoscmd% -h:40 debianrv\ntvao -c -p ttt1.hex >>%outputfile%
%_rvoscmd% -h:40 debianrv\ntvcm -c -p tttcpm.com 1 >>%outputfile%
%_rvoscmd% -h:40 debianrv\ntvdm -c -p ttt8086.com 1 >>%outputfile%

echo %date% %time% >>"%outputfile%"
diff baseline_%outputfile% %outputfile%
