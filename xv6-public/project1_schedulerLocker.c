#include "types.h"
#include "defs.h"
#include "project_define.h"
#include "proc.h"
#include "project1_mlfq.h"

extern struct mlfqs schedmlfq;
// 프로세스 상태 출력
void printProcessState(void) {
    int pid = schedmlfq.nowproc->pid, timequantum = schedmlfq.timequantum, quelevel = schedmlfq.quelevel;
    cprintf("pid: %d, time quantum: %d, level: %d\n", pid, quelevel * 2 + 4 - timequantum, quelevel);
}

// 해당 프로세스가 우선적으로 스케줄링 되도록 함
// password 불일치하면 exit
// yield, sleep 등 무시
void schedulerLock(int password) {
    if(password != PSWORD){
        printProcessState();
        exit();
    }
    // 실행 중인 프로세스에서만 호출
    if(myproc()->pid == schedmlfq.nowproc->pid){
        schedmlfq.islock = 1;
        schedmlfq.ticks = 0;
    }
}

// 해당 프로세스가 우선적으로 스케줄링 되던 것을 중지
void schedulerUnlock(int password) {
    if (password != PSWORD) {
        printProcessState();
        exit();
    }
    // 실행 중인 프로세스에서만 호출
    if (myproc()->pid == schedmlfq.nowproc->pid) {
        schedmlfq.islock = -1;
        yield();
    }
}

// wrapper function
int sys_schedulerLock(void) {
    int psword;
    if (argint(0, &psword) < 0) {
        return -1;
    }
    schedulerLock(psword);
    return 0;
}

// wrapper function
int sys_schedulerUnlock(void) {
    int psword;
    if (argint(0, &psword) < 0) {
        return -1;
    }
    schedulerLock(psword);
    return 0;
}