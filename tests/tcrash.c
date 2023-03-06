#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" int main()
{
    char * pbad = (char *) 0x200;
    *pbad = 10;
} //main

