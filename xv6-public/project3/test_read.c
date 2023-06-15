#include "types.h"
#include "user.h"
#include "fcntl.h"
#define TEST 16777216
char d[100000];

int main(int argc, char *argv[]) {
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        printf(0, "open fail\n");
        exit();
    }
    int offset = atoi(argv[2]);
    int x = TEST, s = 0, ck = 0, e = 0;

    while (x > 0) {
        e = read(fd, d, 100000);
        x -= e;
        s += e;
        if(s > offset && !ck){
            printf(0, "%s\n", d + (offset - s + 100000));
            ck = 1;
        }
        printf(0, "%d\n", s);
    }
    printf(0, "read success\n");
    close(fd);
    exit();
}