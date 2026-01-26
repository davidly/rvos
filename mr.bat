@echo off
del *.pdb 2>nul
cl /DRVOS /W4 /wd4996 /nologo rvos.cxx riscv.cxx /I. /EHsc /DNDEBUG /GS- /GL /Ot /Ox /Ob3 /Oi /Qpar /Zi /Fa /FAs /link /OPT:REF user32.lib

