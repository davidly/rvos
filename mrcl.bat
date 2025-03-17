@echo off
setlocal
path=c:\program files\microsoft visual studio\2022\community\vc\tools\llvm\x64\bin;%path%

rem can't use -Ofast because Nan support won't work
clang++ -DRVOS -DNDEBUG -Wno-psabi -I . -x c++ rvos.cxx riscv.cxx -o rvoscl.exe -O3 -static -fsigned-char -Wno-format -std=c++14 -Wno-deprecated-declarations -luser32.lib
