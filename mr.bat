@echo off
cl /nologo rvos.cxx riscv.cxx /EHsc /DNDEBUG /O2 /Oi /Fa /Qpar /Zi /link /OPT:REF user32.lib

