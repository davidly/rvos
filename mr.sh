# must use -O3 not -Ofast so NAN works
g++ -DRVOS -flto -O3 -D NDEBUG -fno-builtin -I . rvos.cxx riscv.cxx -o rvos -static
