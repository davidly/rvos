@echo off
del rvos.exe
del rvos.pdb
@echo on

cl /nologo rvos.cxx riscv.cxx /MT /Ot /Ox /Ob2 /Oi /Qpar /O2 /EHac /Zi /DNDEBUG /link ntdll.lib user32.lib /OPT:REF

