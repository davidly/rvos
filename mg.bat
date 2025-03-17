@echo off
setlocal

g++ -O3 -ggdb -D _MSC_VER rvos.cxx riscv.cxx -I ../djl -D DEBUG -o rvosg.exe -static


