@echo off
setlocal

g++ -Ofast -ggdb -D _MSC_VER rvos.cxx riscv.cxx -I ../djl -D NDEBUG -o rvosg.exe -static


