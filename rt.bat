@echo off
setlocal

set outputfile=test_rvos.txt
echo %date% %time% >"%outputfile%"

set _testlist=t glob ts sieve e tap tpi tphi tins terrno tp1k ttt^
              tf tm ttime td fileops t_setjmp tex trw empty

( for %%t in (%_testlist%) do ( call :testRun %%t ) )

goto :rustTests

:testRun

echo running test app %~1 using brk heap >>"%outputfile%"
rem using brk heap, so old gcc's libc is ok
rvos -h:10 -m:0 linux\%~1 >>"%outputfile%"

echo running test app %~1 using mmap heap >>"%outputfile%"
rem using mmap heap, so a newer libc is required to not require brk
rvos -h:0 -m:10 linux\%~1 >>"%outputfile%"

exit /b 0

:rustTests

robocopy \\lee-server\documents\scratch\debianrv\rust debianrv\rust * >>%outputfile%

set _testlist=ato real ttt e tap tphi mysort fileops

( for %%t in (%_testlist%) do ( call :testRunRust %%t ) )

goto :singletonTests

:testRunRust

echo running test app %~1 using brk heap >>"%outputfile%"
rvos -h:10 -m:0 debianrv\rust\%~1 >>"%outputfile%"

echo running test app %~1 using mmap >>"%outputfile%"
rvos -h:0 -m:10 debianrv\rust\%~1 >>"%outputfile%"

exit /b 0

:singletonTests

echo running tcrash memory low >>%outputfile%
rvos linux\tcrash ml >>%outputfile%

echo running tcrash memory high >>%outputfile%
rvos linux\tcrash mh >>%outputfile%

echo running tcrash pc low >>%outputfile%
rvos linux\tcrash pcl >>%outputfile%

echo running tcrash pc high >>%outputfile%
rvos linux\tcrash pch >>%outputfile%

echo running tcrash stack pointer low >>%outputfile%
rvos linux\tcrash spl >>%outputfile%

echo running tcrash stack pointer l=high >>%outputfile%
rvos linux\tcrash sph >>%outputfile%

echo running tcrash stack pointer misaligned >>%outputfile%
rvos linux\tcrash spm >>%outputfile%

echo running ba tp.bas >>%outputfile%
rvos linux\ba linux\tp.bas >>%outputfile% >>%outputfile%

echo running ttty tests >>%outputfile%
echo blahblah >bar
rvos linux\ttty >>%outputfile%
rvos linux\ttty <bar >>%outputfile%
echo should see no, yes, yes, yes
rvos linux\ttty <bar 
rvos linux\ttty

echo running mysort >>%outputfile%
del linux\sorted.txt
rvos /h:2 linux\mysort /q /u linux\words.txt linux\sorted.txt >>%outputfile%
head linux\sorted.txt >>%outputfile%

echo running an with brk>>%outputfile%
rvos -h:40 -m:0 linux\an david lee >>%outputfile%

echo running an with mmap >>%outputfile%
rvos -h:0 -m:40 debianrv\an phoebe bridgers >>%outputfile%

diff baseline_%outputfile% %outputfile%

:eof
