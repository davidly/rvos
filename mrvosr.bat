@echo off
setlocal

rem  -O<number>                  Set optimization level to <number>.
rem  -Ofast                      Optimize for speed disregarding exact standards compliance.
rem  -Og                         Optimize for debugging experience rather than speed or size.
rem  -Os                         Optimize for space rather than speed.

set OPTFLAGS=-Ofast

path=C:\Users\david\OneDrive\riscv-gcc\bin;%path%
path=C:\Users\david\OneDrive\riscv-gcc\bin\riscv64-unknown-elf;%path%
path=c:\users\david\onedrive\riscv-gcc\riscv64-unknown-elf\bin;%path%
path=C:\Users\david\OneDrive\riscv-gcc\bin\libexec\gcc\riscv64-unknown-elf\8.2.0;%path%

rem RVC is instruction compression for risc-v -- 16 bit compact-opcodes for many 32-bit opcodes
set RVCFLAG=c

riscv64-unknown-elf-g++ ^
  rvos.cxx riscv.cxx ^
  -I . -I ..\djl -D NDEBUG -D OLDGCC ^
  -mcmodel=medany -mabi=lp64f -march=rv64imaf%RVCFLAG% -fno-common -ffunction-sections -fdata-sections -fstrict-volatile-bitfields ^
  -fno-zero-initialized-in-bss %OPTFLAGS% -ggdb -static -Wl,--gc-sections -Wl,-static -Wl,--whole-archive -Wl,--no-whole-archive ^
  -Wl,-EL -Wl,--no-relax -T "rvos.ld" ^
  "tests\rvosutil.o" "tests\rvos_shell.o" ^
  -o "rvos.elf" -Wl,--start-group -lgcc -lm -lc -Wl,-lgcc -lm -lc -Wl,--end-group


