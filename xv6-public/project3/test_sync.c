#include "types.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[]) {
    int fd = open(argv[1], O_CREATE | O_WRONLY);
    if (fd == -1) {
        printf(0, "open fail\n");
        exit();
    }

    int length = atoi(argv[3]);
    char d = 'a';

    // sync
    if(strcmp(argv[2], "s") == 0){
        for(int i=0;i<length;i++)
            write(fd, &d, 1);
        printf(0, "%d\n", sync());
    }else if(strcmp(argv[2], "n") == 0){
        for(int i=0;i<length;i++)
            write(fd, &d, 1);
    }else{
        printf(0, "format error\n");
        exit();
    }
    close(fd);
    exit();
}