@echo off
del *.pdb 2>nul
cl /DRVOS /nologo rvos.cxx riscv.cxx /I. /EHsc /DDEBUG /O2 /Oi /Fa /Qpar /Zi /link /OPT:REF user32.lib


