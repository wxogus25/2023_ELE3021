#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_LOOP 100000
#define NUM_YIELD 20000
#define NUM_SLEEP 1000

#define NUM_THREAD 4
#define MAX_LEVEL 5

int parent;

int fork_children() {
    int i, p;
    for (i = 0; i < NUM_THREAD; i++)
        if ((p = fork()) == 0) {
            sleep(10);
            return getpid();
        }
    return parent;
}

int fork_children2() {
    int i, p;
    for (i = 0; i < NUM_THREAD; i++) {
        if ((p = fork()) == 0) {
            sleep(300);
            p = getpid();
            setPriority(p, i);
            return p;
        }
    }
    return parent;
}

int max_level;

int fork_children3() {
    int i, p;
    for (i = 0; i < NUM_THREAD; i++) {
        if ((p = fork()) == 0) {
            sleep(10);
            schedulerLock(2018009116);
            return getpid();
        }
    }
    printf(1, "parent\n");
    schedulerUnlock(2018009116);
    return parent;
}
void exit_children() {
    printf(1, "exit unlock test\n");
    schedulerLock(2018009116);
    if (getpid() != parent) exit();
    while (wait() != -1)
        ;
}

int main(int argc, char *argv[]) {
    int i, pid;
    int count[MAX_LEVEL] = {0};
    //  int child;

    parent = getpid();
    if(argv[1][0] == 'c'){
        schedulerLock(201135);
    }
    printf(1, "MLFQ test start\n");

    printf(1, "[Test 3] default\n");
    schedulerLock(2018009116);
    sleep(10);
    schedulerUnlock(2018009116);
    pid = fork_children3();

    if (pid != parent) {
        for (i = 0; i < NUM_LOOP; i++) {
            int x = getLevel();
            if (x < 0 || x > 4) {
                printf(1, "Wrong level: %d\n", x);
                exit();
            }
            count[x]++;
        }
        printf(1, "Process %d\n", pid);
        for (i = 0; i < MAX_LEVEL; i++) printf(1, "L%d: %d\n", i, count[i]);
    }
    exit_children();
    printf(1, "[Test 3] finished\n");
    printf(1, "done\n");
    exit();
}
