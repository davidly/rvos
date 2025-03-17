@echo off
setlocal

rem can't use -Ofast because NaN support won't work
g++ -O3 -ggdb -D RVOS -D _MSC_VER rvos.cxx riscv.cxx -I ../djl -D NDEBUG -o rvosg.exe -static


