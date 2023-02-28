rem @echo off
setlocal

path=c:\Users\david\AppData\Local\arduino15\packages\maixduino\tools\riscv64-unknown-elf-gcc\8.2.0_20190409\riscv64-unknown-elf\bin;%path%
path=c:\Users\david\AppData\Local\arduino15\packages\maixduino\tools\riscv64-unknown-elf-gcc\8.2.0_20190409\bin;%path%
path=c:\users\david\appdata\local\arduino15\packages\maixduino\tools\riscv64-unknown-elf-gcc\8.2.0_20190409\libexec\gcc\riscv64-unknown-elf\8.2.0\;%path%

rem RVC is instruction compression for risc-v -- 16 bit compact-opcodes for many 32-bit opcodes
set RVCFLAG=c

as -a=riscv_shell.lst -mabi=lp64f -march=rv64imaf%RVCFLAG% -fpic riscv_shell.s -o riscv_shell.o

rem as -a=%1.lst -mabi=lp64f -march=rv64imaf%RVCFLAG% -fpic %1.s -o %1.o

riscv64-unknown-elf-g++ ^
   %1.c ^
  -mcmodel=medany -mabi=lp64f -march=rv64imaf%RVCFLAG% -fno-common -ffunction-sections -fdata-sections -fstrict-volatile-bitfields ^
  -fno-zero-initialized-in-bss -Os -ggdb -nostartfiles -static -Wl,--gc-sections -Wl,-static -Wl,--whole-archive -Wl,--no-whole-archive ^
  -Wl,-EL -Wl,--no-relax -T "kendryte.ld" "riscv_shell.o" -o "%1.elf" -Wl,--start-group -lgcc -lm -lc -Wl,-lgcc -lm -lc -Wl,--end-group


riscv64-unknown-elf-g++ ^
   %1.c ^
  -mcmodel=medany -mabi=lp64f -march=rv64imaf%RVCFLAG% -fno-common -ffunction-sections -fdata-sections -fstrict-volatile-bitfields ^
  -fno-zero-initialized-in-bss -Os -ggdb -nostartfiles -static -Wl,--gc-sections -Wl,-static -Wl,--whole-archive -Wl,--no-whole-archive ^
  -Wl,-EL -Wl,--no-relax -T "kendryte.ld" -o "%1.lst" -Wl,--start-group -lgcc -lm -lc -Wl,-lgcc -lm -lc -Wl,--end-group -S

