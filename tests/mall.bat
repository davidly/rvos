@echo off
setlocal

set _testlist=t tbad glob tenv ts tf tap ttt sieve e an tpi td ba mysort^
              tphi tcrash ttime tm terrno fileops empty

( for %%t in (%_testlist%) do ( call mariscv %%t ) )

call mta tins

..\rvos ba /a:r tp1k.bas /x /q
call ma tp1k

..\rvos ba /a:r tp.bas /x /q
call ma tp

