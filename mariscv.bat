@echo off
setlocal

path=c:\Users\david\AppData\Local\arduino15\packages\maixduino\tools\riscv64-unknown-elf-gcc\8.2.0_20190409\riscv64-unknown-elf\bin;%path%
path=c:\Users\david\AppData\Local\arduino15\packages\maixduino\tools\riscv64-unknown-elf-gcc\8.2.0_20190409\bin;%path%
path=c:\users\david\appdata\local\arduino15\packages\maixduino\tools\riscv64-unknown-elf-gcc\8.2.0_20190409\libexec\gcc\riscv64-unknown-elf\8.2.0\;%path%

as -a=%1.lst -mabi=lp64f -march=rv64imaf -fpic %1.s -o %1.o
as -a=riscv_shell.lst -mabi=lp64f -march=rv64imaf -fpic riscv_shell.s -o riscv_shell.o

riscv64-unknown-elf-g++ ^
  -mcmodel=medany -mabi=lp64f -march=rv64imaf -fno-common -ffunction-sections -fdata-sections -fstrict-volatile-bitfields ^
  -fno-zero-initialized-in-bss -Os -ggdb -nostartfiles -static -Wl,--gc-sections -Wl,-static -Wl,--whole-archive -Wl,--no-whole-archive ^
  -Wl,-EL -Wl,--no-relax -T "kendryte.ld" "riscv_shell.o" "%1.o" -o "%1.elf" -Wl,--start-group -lgcc -lm -lc -Wl,-lgcc -lm -lc -Wl,--end-group

