@echo off
setlocal

rem any command-line argument means run rvos nested in rvos
if "%1" == "" (set _rvoscmd=rvos) else (set _rvoscmd=rvos -h:60 linux\rvos)

set outputfile=test_rvos.txt
echo %date% %time% >"%outputfile%"

set _testlist=t glob ts sieve e tap tpi tphi tins terrno tp ttt^
              tf tm ttime td fileops t_setjmp tex trw empty

( for %%t in (%_testlist%) do ( call :testRun %%t ) )

goto :rustTests

:testRun

echo running test app %~1 using brk heap >>"%outputfile%"
rem using brk heap, so old gcc's libc is ok
%_rvoscmd% -h:10 -m:0 linux\%~1 >>"%outputfile%"

echo running test app %~1 using mmap heap >>"%outputfile%"
rem using mmap heap, so a newer libc is required to not require brk
%_rvoscmd% -h:0 -m:10 linux\%~1 >>"%outputfile%"

exit /b 0

:rustTests

robocopy \\lee-server\documents\scratch\debianrv\rust debianrv\rust * >>%outputfile%

set _testlist=ato real ttt e tap tphi mysort fileops

( for %%t in (%_testlist%) do ( call :testRunRust %%t ) )

goto :singletonTests

:testRunRust

echo running test app %~1 using brk heap >>"%outputfile%"
%_rvoscmd% -h:10 -m:0 debianrv\rust\%~1 >>"%outputfile%"

echo running test app %~1 using mmap >>"%outputfile%"
%_rvoscmd% -h:0 -m:10 debianrv\rust\%~1 >>"%outputfile%"

exit /b 0

:singletonTests

echo running ttt_riscv >>%outputfile%
%_rvoscmd% -p ttt_riscv.elf 1 >>%outputfile%

echo running ttt_rvu >>%outputfile%
%_rvoscmd% -p ttt_rvu.elf 1 >>%outputfile%

echo running tmmap >>%outputfile%
%_rvoscmd% -h:0 -m:20 debianrv\tmmap >>%outputfile%

echo running tcrash memory low >>%outputfile%
%_rvoscmd% linux\tcrash ml >>%outputfile%

echo running tcrash memory high >>%outputfile%
%_rvoscmd% linux\tcrash mh >>%outputfile%

echo running tcrash pc low >>%outputfile%
%_rvoscmd% linux\tcrash pcl >>%outputfile%

echo running tcrash pc high >>%outputfile%
%_rvoscmd% linux\tcrash pch >>%outputfile%

echo running tcrash stack pointer low >>%outputfile%
%_rvoscmd% linux\tcrash spl >>%outputfile%

echo running tcrash stack pointer l=high >>%outputfile%
%_rvoscmd% linux\tcrash sph >>%outputfile%

echo running tcrash stack pointer misaligned >>%outputfile%
%_rvoscmd% linux\tcrash spm >>%outputfile%

echo running ba tp.bas >>%outputfile%
%_rvoscmd% linux\ba linux\tp.bas >>%outputfile% >>%outputfile%

echo running ttty tests >>%outputfile%
echo blahblah >bar
%_rvoscmd% linux\ttty >>%outputfile%
%_rvoscmd% linux\ttty <bar >>%outputfile%
echo should see no, yes, yes, yes
%_rvoscmd% linux\ttty <bar 
%_rvoscmd% linux\ttty

echo running mysort >>%outputfile%
del linux\sorted.txt
%_rvoscmd% -h:2 linux\mysort /q /u linux\words.txt linux\sorted.txt >>%outputfile%
head linux\sorted.txt >>%outputfile%

echo running an with brk>>%outputfile%
%_rvoscmd% -h:40 -m:0 linux\an david lee >>%outputfile%

echo running an with mmap >>%outputfile%
%_rvoscmd% -h:0 -m:40 debianrv\an phoebe bridgers >>%outputfile%

echo running ntvao, ntvcm, and ntvdm >>%outputfile%
%_rvoscmd% -h:40 linux\ntvao -c -p ttt1.hex >>%outputfile%
%_rvoscmd% -h:40 linux\ntvcm -c -p tttcpm.com 1 >>%outputfile%
%_rvoscmd% -h:40 linux\ntvdm -c -p ttt8086.com 1 >>%outputfile%

echo %date% %time% >>"%outputfile%"
diff baseline_%outputfile% %outputfile%
