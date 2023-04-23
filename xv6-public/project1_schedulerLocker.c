#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "spinlock.h"
#include "project_define.h"
#include "project1_mlfq.h"

extern struct mlfqs schedmlfq;
// 프로세스 상태 출력
void
printProcessState(void)
{
  int pid = schedmlfq.nowproc->pid, timequantum = schedmlfq.timequantum, quelevel = schedmlfq.quelevel;
  cprintf("pid: %d, time quantum: %d, level: %d\n", pid, quelevel * 2 + 4 - timequantum, quelevel);
}

// 해당 프로세스가 우선적으로 스케줄링 되도록 함
// password 불일치하면 exit
// yield, sleep 등 무시
void
schedulerLock(int password)
{
  if(password != PSWORD){
    printProcessState();
    exit();
  }
  // 실행 중인 프로세스에서만 호출
  if(myproc()->pid == schedmlfq.nowproc->pid){
    if(schedmlfq.islock == 0){
      if(schedmlfq.lockproc == 0){
        //cprintf("lock success\n");
        schedmlfq.islock = 1;
        schedmlfq.ticks = 1;
        schedmlfq.lockproc = schedmlfq.nowproc;
      }else{
        //cprintf("lock error\n");
      }
    }else{
      if (schedmlfq.lockproc != schedmlfq.nowproc){
        //cprintf("how can do this?\n");
      }else{
        //cprintf("lock duplication\n");
      }
    }
  }else{
    //cprintf("external access schedulerLock\n");
  }
}

// 해당 프로세스가 우선적으로 스케줄링 되던 것을 중지
void
schedulerUnlock(int password)
{
  //cprintf("unlock access\n");
  if (password != PSWORD) {
    printProcessState();
    exit();
  }
  // 실행 중인 프로세스에서만 호출
  if (myproc()->pid == schedmlfq.nowproc->pid) {
    if(schedmlfq.islock == 1){
      if(schedmlfq.lockproc == schedmlfq.nowproc){
        //cprintf("unlock success\n");
        schedmlfq.islock = -1;
        schedmlfq.lockproc = 0;
        yield();
      }else{
        //cprintf("unlock impossible\n");
      }
    }else{
      if(schedmlfq.lockproc != 0){
        //cprintf("unlock fail %d\n",schedmlfq.lockproc->pid);
      }else{
        //cprintf("unused unlock %d\n", schedmlfq.islock);
      }
    }
  }else{
    //cprintf("external access schedulerUnock\n");
  }
}

// wrapper function
int
sys_schedulerLock(void)
{
  int psword;
  if (argint(0, &psword) < 0) {
    return -1;
  }
  schedulerLock(psword);
  return 0;
}

// wrapper function
int
sys_schedulerUnlock(void)
{
  int psword;
  if (argint(0, &psword) < 0) {
    return -1;
  }
  schedulerUnlock(psword);
  return 0;
}