@echo off
cl /W4 /wd4996 /nologo rvos.cxx riscv.cxx /I. /EHsc /DNDEBUG /O2 /GL /Oi /Fa /Qpar /Zi /link /OPT:REF user32.lib

