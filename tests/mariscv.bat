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
rem set RVCFLAG=c

rem build the rvos shell _start and abi

as -a=rvos_shell.lst -mabi=lp64f -march=rv64imaf%RVCFLAG% -fpic rvos_shell.s -o rvos_shell.o

riscv64-unknown-elf-g++ ^
  rvosutil.c -c ^
  -I .. ^
  -mcmodel=medany -mabi=lp64f -march=rv64imaf%RVCFLAG% -fno-common -ffunction-sections -fdata-sections -fstrict-volatile-bitfields ^
  -fno-zero-initialized-in-bss -Os -ggdb -static -Wl,--gc-sections -Wl,-static -Wl,--whole-archive -Wl,--no-whole-archive ^
  -Wl,-EL -Wl,--no-relax -o "rvosutil.o" -Wl,--start-group -lgcc -lm -lc -Wl,-lgcc -lm -lc -Wl,--end-group

rem generate a listing if needed
rem riscv64-unknown-elf-g++ ^
rem   %1.c ^
rem   -I .. ^
rem   -mcmodel=medany -mabi=lp64f -march=rv64imaf%RVCFLAG% -fno-common -ffunction-sections -fdata-sections -fstrict-volatile-bitfields ^
rem   -fno-zero-initialized-in-bss %OPTFLAGS% -ggdb -static -Wl,--gc-sections -Wl,-static -Wl,--whole-archive -Wl,--no-whole-archive ^
rem   -Wl,-EL -Wl,--no-relax -o "%1.lst" -Wl,--start-group -lgcc -lm -lc -Wl,-lgcc -lm -lc -Wl,--end-group -S

riscv64-unknown-elf-g++ ^
  %1.c ^
  -I .. ^
  -mcmodel=medany -mabi=lp64f -march=rv64imaf%RVCFLAG% -fno-common -ffunction-sections -fdata-sections -fstrict-volatile-bitfields ^
  -fno-zero-initialized-in-bss %OPTFLAGS% -ggdb -static -Wl,--gc-sections -Wl,-static -Wl,--whole-archive -Wl,--no-whole-archive ^
  -Wl,-EL -Wl,--no-relax ^
  "rvosutil.o" "rvos_shell.o" -T rvos.ld ^
  -o "%1.elf" -Wl,--start-group -lgcc -lm -lc -Wl,-lgcc -lm -lc -Wl,--end-group


