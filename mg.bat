@echo off
setlocal

path=d:\mingw64\bin;%path%

g++ -Ofast -ggdb -D _MSC_VER rvos.cxx riscv.cxx -I ../djl -D DEBUG -o rvos.exe


