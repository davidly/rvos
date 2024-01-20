@echo off
setlocal

set outputfile=test_rvos.txt
echo %date% %time% >"%outputfile%"

set _testlist=t glob ts sieve e tap tpi tphi tins terrno tp1k ttt^
              tf tm ttime td fileops empty

( for %%t in (%_testlist%) do ( call :testRun %%t ) )

goto :rustTests

:testRun

echo running test app %~1 using brk heap >>"%outputfile%"
rem using brk heap, so old gcc's libc is ok
rvos -h:10 -m:0 tests\%~1 >>"%outputfile%"

echo running test app %~1 using mmap heap >>"%outputfile%"
rem using mmap heap, so a newer libc is required to not require brk
rvos -h:0 -m:10 linux\%~1 >>"%outputfile%"

exit /b 0

:rustTests

robocopy \\lee-server\documents\scratch\debianrv\rust debianrv\rust * >>%outputfile%

set _testlist=ato real ttt e tap tphi

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
rvos tests\tcrash ml >>%outputfile%

echo running tcrash memory high >>%outputfile%
rvos tests\tcrash mh >>%outputfile%

echo running tcrash pc low >>%outputfile%
rvos tests\tcrash pcl >>%outputfile%

echo running tcrash pc high >>%outputfile%
rvos tests\tcrash pch >>%outputfile%

echo running tcrash stack pointer low >>%outputfile%
rvos tests\tcrash spl >>%outputfile%

echo running tcrash stack pointer l=high >>%outputfile%
rvos tests\tcrash sph >>%outputfile%

echo running tcrash stack pointer misaligned >>%outputfile%
rvos tests\tcrash spm >>%outputfile%

echo running ba tp.bas >>%outputfile%
rvos tests\ba tests\tp.bas >>%outputfile% >>%outputfile%

echo running mysort >>%outputfile%
del tests\sorted.txt
rvos /h:2 tests\mysort /q /u tests\words.txt tests\sorted.txt >>%outputfile%
head tests\sorted.txt >>%outputfile%

echo running an with brk>>%outputfile%
rvos -h:40 -m:0 tests\an david lee >>%outputfile%

echo running an with mmap >>%outputfile%
rvos -h:0 -m:40 debianrv\an phoebe bridgers >>%outputfile%

diff baseline_%outputfile% %outputfile%

:eof
