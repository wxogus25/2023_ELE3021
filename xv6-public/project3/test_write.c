#include "types.h"
#include "user.h"
#include "fcntl.h"
#define TEST 16777216
char d[100000];

int main(int argc, char *argv[]) {
    int fd = open(argv[1], O_CREATE | O_WRONLY);
    if (fd == -1) {
        printf(0, "open fail\n");
        exit();
    }
    memset(d, -1, 100000);

    int offset = atoi(argv[2]);
    int len = strlen(argv[3]) + 1;
    int x = TEST, s = 0, ck = 0, e = 0;

    while(x > 0){
        if(s + 100000 >= offset && !ck){
            int e1, e2, e3;
            e1 = write(fd, d, offset - s);
            e2 = write(fd, argv[3], len);
            e3 = write(fd, d, 100000 - (offset - s + len));
            ck = 1;
            e = e1 + e2 + e3;
            x -= e;
            s += e;
        }else{
            e = write(fd, d, 100000);
            x -= e;
            s += e;
        }
        printf(0, "%d\n", s);
    }

    printf(0, "write success\n");
    close(fd);
    exit();
}