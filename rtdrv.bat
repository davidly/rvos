@echo off
setlocal

set outputfile=testdrv_rvos.txt
echo %date% %time% >"%outputfile%"

robocopy \\lee-server\documents\scratch\debianrv debianrv * /s >>%outputfile%

set _testlist=t glob ts sieve e tap tpi tphi tins terrno tp1k ttt tf^
              tm ttime td fileops t_setjmp tex trw empty

( for %%t in (%_testlist%) do ( call :testRun %%t ) )

goto :runRustTests

:testRun

echo running test app %~1 using brk heap >>"%outputfile%"
rem using brk heap, so old gcc's libc is ok
rvos -h:10 -m:0 debianrv\%~1 >>"%outputfile%"

echo running test app %~1 using mmap heap >>"%outputfile%"
rem using mmap heap, so a newer libc is required to not require brk
rvos -h:0 -m:10 debianrv\%~1 >>"%outputfile%"

exit /b 0

:runRustTests

set _testlist=ato real ttt e tap tphi mysort fileops

( for %%t in (%_testlist%) do ( call :testRunRust %%t ) )

goto :runSingletonTests

:testRunRust

echo running test app %~1 using brk heap >>"%outputfile%"
rvos -h:10 -m:0 debianrv\rust\%~1 >>"%outputfile%"

echo running test app %~1 using mmap >>"%outputfile%"
rvos -h:0 -m:10 debianrv\rust\%~1 >>"%outputfile%"

exit /b 0

:runSingletonTests

rvos -h:0 -m:20 debianrv\tmmap >>%outputfile%

rvos debianrv\tcrash ml >>%outputfile%
rvos debianrv\tcrash mh >>%outputfile%
rvos debianrv\tcrash pcl >>%outputfile%
rvos debianrv\tcrash pch >>%outputfile%
rvos debianrv\tcrash spl >>%outputfile%
rvos debianrv\tcrash sph >>%outputfile%
rvos debianrv\tcrash spm >>%outputfile%

echo running ttty tests >>%outputfile%
echo blahblah >bar
rvos debianrv\ttty >>%outputfile%
rvos debianrv\ttty <bar >>%outputfile%
echo should see no, yes, yes, yes
rvos debianrv\ttty <bar 
rvos debianrv\ttty

rvos debianrv\ba debianrv\tp.bas >>%outputfile%

rvos /h:2 debianrv\mysort /q /u debianrv\words.txt debianrv\sorted.txt >>%outputfile%
rvos /h:40 debianrv\an david lee >>%outputfile%

rvos -h:30 debianrv\rvos debianrv\tp.elf 1 >>%outputfile%
rvos -h:30 debianrv\ntvao -c ../ntvao/ttt1.hex >>%outputfile%
rvos -h:30 debianrv\ntvcm -c ../ntvcm/tttcpm.com 1 >>%outputfile%
rvos -h:30 debianrv\ntvdm -c ../ntvdm/ttt8086.com 1 >>%outputfile%

diff baseline_%outputfile% %outputfile%
