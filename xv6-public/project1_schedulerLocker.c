#include "types.h"
#include "defs.h"
#include "project_define.h"
#include "proc.h"
#include "project1_mlfq.h"

extern struct mlfq *mlfqs;
// 프로세스 상태 출력
void printProcessState(void) {
    struct proc *p = myproc();
    struct proc_w *procwrap = findprocwrap(p);
    cprintf("pid: %d, time quantum: %d, level: %d\n", p->pid, procwrap->timequantum, procwrap->quelevel);
}

// 해당 프로세스가 우선적으로 스케줄링 되도록 함
// password 불일치하면 exit
void schedulerLock(int password) {
    if(password != PSWORD){
        printProcessState();
        exit();
    }
    // TODO
}

// 해당 프로세스가 우선적으로 스케줄링 되던 것을 중지
void schedulerUnlock(int password) {
    if (password != PSWORD) {
        printProcessState();
        exit();
    }
    // TODO
}

// wrapper function
int sys_schedulerLock(void) {
    int psword;
    if (fetchint(0, &psword) < 0) {
        return -1;
    }
    schedulerLock(psword);
    return 0;
}

// wrapper function
int sys_schedulerUnlock(void) {
    int psword;
    if (fetchint(0, &psword) < 0) {
        return -1;
    }
    schedulerLock(psword);
    return 0;
}