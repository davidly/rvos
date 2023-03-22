@echo off
cl /nologo rvos.cxx riscv.cxx /I. /EHsc /DDEBUG /O2 /Oi /Fa /Qpar /Zi /link /OPT:REF user32.lib


