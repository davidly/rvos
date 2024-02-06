@echo off
cl /W4 /wd4996 /nologo rvos.cxx riscv.cxx /I. /EHsc /DNDEBUG /GS- /GL /Ot /Ox /Ob3 /Oi /Qpar /Zi /Fa /FAsc /link /OPT:REF user32.lib

