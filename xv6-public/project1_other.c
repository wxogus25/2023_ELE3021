#include "types.h"
#include "defs.h"
#include "project_define.h"
#include "proc.h"
#include "project1_mlfq.h"

extern struct mlfqs schedmlfq;
// 다음 프로세스에세 프로세서 양보
void myYield(void) {
    yield();
}

// 프로세스가 속한 큐의 레벨 반환
int getLevel(void) {
    return schedmlfq.quelevel;
}

// 해당 pid의 프로세스의 priority 설정
void setPriority(int pid, int priority) {
    if(pid == schedmlfq.nowproc->pid)
        schedmlfq.priority = priority;
}

// wrapper function
int sys_myYield(void) {
    myYield();
    return 0;
}

// wrapper function
int sys_getLevel(void) {
    return getLevel();
}

// wrapper function
int sys_setPriority(void) {
    int pid, priority;
    if(argint(0, &pid) < 0 || argint(1, &priority)){
        return -1;
    }
    setPriority(pid, priority);
    return 0;
}