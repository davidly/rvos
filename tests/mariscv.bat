@echo off
setlocal

path=C:\Users\david\OneDrive\riscv-gcc\bin;%path%
path=C:\Users\david\OneDrive\riscv-gcc\bin\riscv64-unknown-elf;%path%
path=c:\users\david\onedrive\riscv-gcc\riscv64-unknown-elf\bin;%path%
path=C:\Users\david\OneDrive\riscv-gcc\bin\libexec\gcc\riscv64-unknown-elf\8.2.0;%path%

rem RVC is instruction compression for risc-v -- 16 bit compact-opcodes for many 32-bit opcodes
rem set RVCFLAG=c

rem build the riscv shell with riscv_print_text()

as -a=riscv_shell.lst -mabi=lp64f -march=rv64imaf%RVCFLAG% -fpic riscv_shell.s -o riscv_shell.o

rem build riscv_printf() helper function

rem -nostartfiles

riscv64-unknown-elf-g++ ^
   prt.c -c ^
  -mcmodel=medany -mabi=lp64f -march=rv64imaf%RVCFLAG% -fno-common -ffunction-sections -fdata-sections -fstrict-volatile-bitfields ^
  -fno-zero-initialized-in-bss -Os -ggdb -static -Wl,--gc-sections -Wl,-static -Wl,--whole-archive -Wl,--no-whole-archive ^
  -Wl,-EL -Wl,--no-relax -T "kendryte.ld" -o "prt.o" -Wl,--start-group -lgcc -lm -lc -Wl,-lgcc -lm -lc -Wl,--end-group

rem generate a listing if needed
rem riscv64-unknown-elf-g++ ^
rem    %1.c ^
rem   -mcmodel=medany -mabi=lp64f -march=rv64imadf%RVCFLAG% -fno-common -ffunction-sections -fdata-sections -fstrict-volatile-bitfields ^
rem   -fno-zero-initialized-in-bss -Os -ggdb -static -Wl,--gc-sections -Wl,-static -Wl,--whole-archive -Wl,--no-whole-archive ^
rem   -Wl,-EL -Wl,--no-relax -T "kendryte.ld" -o "%1.lst" -Wl,--start-group -lgcc -lm -lc -Wl,-lgcc -lm -lc -Wl,--end-group -S

riscv64-unknown-elf-g++ ^
   %1.c ^
  -mcmodel=medany -mabi=lp64f -march=rv64imaf%RVCFLAG% -fno-common -ffunction-sections -fdata-sections -fstrict-volatile-bitfields ^
  -fno-zero-initialized-in-bss -Os -ggdb -static -Wl,--gc-sections -Wl,-static -Wl,--whole-archive -Wl,--no-whole-archive ^
  -Wl,-EL -Wl,--no-relax -T "kendryte.ld" ^
  "prt.o" "riscv_shell.o" ^
  -o "%1.elf" -Wl,--start-group -lgcc -lm -lc -Wl,-lgcc -lm -lc -Wl,--end-group


