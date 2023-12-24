@echo off
del rvos.exe
del rvos.pdb
@echo on

rem cl /nologo rvos.cxx riscv.cxx /I . /MT /Ot /Ox /Ob2 /Oi /Qpar /O2 /EHac /Zi /DNDEBUG /link ntdll.lib /OPT:REF
cl /nologo rvos.cxx riscv.cxx /I . /MT /Ot /Ox /Ob2 /Oi /Qpar /O2 /EHac /Zi /DDEBUG /link ntdll.lib user32.lib /OPT:REF

