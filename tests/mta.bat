@echo off
setlocal

path=C:\Users\david\OneDrive\riscv-gcc\bin;%path%
path=C:\Users\david\OneDrive\riscv-gcc\bin\riscv64-unknown-elf;%path%
path=c:\users\david\onedrive\riscv-gcc\riscv64-unknown-elf\bin;%path%
path=C:\Users\david\OneDrive\riscv-gcc\bin\libexec\gcc\riscv64-unknown-elf\8.2.0;%path%

rem RVC is instruction compression for risc-v -- 16 bit compact-opcodes for many 32-bit opcodes
set RVCFLAG=c

as -a=%1.lst -mabi=lp64f -march=rv64imaf%RVCFLAG% --defsym OLDGCC=1 -fpic %1.s -o %1.o

riscv64-unknown-elf-g++ ^
  -mcmodel=medany -mabi=lp64f -march=rv64imaf%RVCFLAG% -fno-common -ffunction-sections -fdata-sections -fstrict-volatile-bitfields ^
  -fno-zero-initialized-in-bss -Os -ggdb -static -Wl,--gc-sections -Wl,-static -Wl,--whole-archive -Wl,--no-whole-archive ^
  -Wl,-EL -Wl,--no-relax -T "kendryte.ld" ^
  %1.o ^
  -o "%1.elf" -Wl,--start-group -lgcc -lm -lc -Wl,-lgcc -lm -lc -Wl,--end-group


