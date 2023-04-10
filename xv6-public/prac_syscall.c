#include "types.h"
#include "defs.h"

// kernel function
int myfunction(char* str){
    cprintf("%s\n", str);
    return 0xABCDABCD;
}

// wrapper function
int sys_myfunction(void){
    char* str;

    // get str from process stack, return length
    if(argstr(0, &str) < 0)
        return -1;
    return myfunction(str);
}