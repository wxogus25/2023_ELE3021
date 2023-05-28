#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"
#include "spinlock.h"

extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

extern struct proc *initproc;

// fork와 비슷한 방식으로 동작
int thread_create(thread_t *thread, void *(*start_rootine)(void *), void *arg){
  int i, idx = -1, pass = 0, cnt = 0;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0) {
    cprintf("allocproc error\n");
    return -1;
  }

  acquire(&ptable.lock);

  // main thread 선택
  if(curproc->mainthread != 0){
    curproc = curproc->mainthread;
  }

  // main thread의 tid는 스레드의 개수를 나타냄
  np->tid = curproc->tid;
  // 만약 이전에 할당받은 스레드 번호이면 추가 할당 없이 진행
  if (curproc->thd[np->tid] == 0) {
    // 그렇지 않으면 새롭게 할당받을 페이지 선정
    for (i = 0; i < MAXPAGE; i++) {
      if (curproc->tstack[i] == 0) {
        idx = i;
        curproc->thd[np->tid] = i + 1;
        curproc->tstack[i] = 1;
        break;
      }
      cnt++;
    }
    // 메모리 제한 초과하면 실행 안 됨
    if (curproc->memlimit != 0 && curproc->memlimit < curproc->sz + PGSIZE) {
      cprintf("memlimit error\n");
      return -1;
    }
  } else {
    pass = 1;
    idx = curproc->thd[np->tid] - 1;
  }

  // 선택된 페이지 없으면 panic
  if(idx == -1){
    cprintf("sz : %d, cnt : %d\n", curproc->sz, cnt);
    panic("need more memory");
    return -1;
  }

  // np의 thread 값 설정
  np->mainthread = curproc;
  np->tid += 1;
  *thread = np->tid;

  // np의 proc 값 설정
  np->pgdir = curproc->pgdir;
  *np->tf = *curproc->tf;
  np->parent = curproc->parent;
  np->pid = curproc->pid;
  np->memlimit = curproc->memlimit;
  np->stacksize = curproc->stacksize;
  np->base = curproc->base;

  // 이미 할당된 페이지면 넘어가고 아니면 새로 할당
  if(pass == 0){
    if((np->sz = allocuvm(np->pgdir, curproc->base + idx * PGSIZE, curproc->base + (idx + 1) * PGSIZE)) == 0){
      curproc->tstack[idx - 1] = 0;
      np->state = UNUSED;
      release(&ptable.lock);
      cprintf("allocuvm error\n");
      return -1;
    }
  }else{
    np->sz = curproc->sz;
  }

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  // np의 stack, tf 설정
  uint ustack[2];
  ustack[0] = 0xffffffff;
  ustack[1] = (uint)arg;

  // start_rootine 실행을 위해 스택에 값 지정해줌
  if (copyout(np->pgdir, np->sz - 8, ustack, 8) < 0) {
    deallocuvm(np->pgdir, np->sz + (idx + 1) * PGSIZE, np->sz + idx * PGSIZE);
    curproc->tstack[idx - 1] = 0;
    np->state = UNUSED;
    release(&ptable.lock);
    cprintf("copyout error\n");
    return -1;
  }

  // 페이지를 새로 할당 받아서 크기가 main thread보다 크면 다른 스레드도 다 sz 변경
  if(np->sz > curproc->sz){
    curproc->sz = np->sz;
    for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if (p->pid == curproc->pid)
        p->sz = curproc->sz;
    }
  }

  // 스레드 개수 하나 추가
  curproc->tid += 1;

  // fd 설정
  np->cwd = idup(curproc->cwd);
  for (i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);

  // 함수가 호출된 것처럼 되도록 설정
  np->tf->eip = (uint)start_rootine;
  np->tf->esp = np->sz - 8;
  np->state = RUNNABLE;

  release(&ptable.lock);

  return 0;
}

// exit와 비슷한 방식으로 동작
void thread_exit(void *retval){
  struct proc *curproc = myproc();

  if (curproc == initproc) panic("init exiting");

  // fd ref 감소
  for (int fd = 0; fd < NOFILE; fd++) {
    if (curproc->ofile[fd]) {
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }
  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  // return value 설정
  curproc->retval = retval;

  acquire(&ptable.lock);

  // 해당 스레드를 부모로 둔 프로세스 init에 붙임
  for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->parent == curproc) {
      p->parent = initproc;
      if (p->state == ZOMBIE) wakeup1(initproc);
    }
  }

  // 부모가 아닌 join으로 대기 중인 main thread를 깨움
  wakeup1(curproc->mainthread);

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  // thread 수 감소
  curproc->mainthread->tid--;
  sched();
  panic("zombie exit");
}

// 메인 스레드에서 함수 호출
// 해당 스레드가 종료 될 때까지 확인하며 종료되면 결과 retval에 저장
// wait 처럼 자원 반환
int thread_join(thread_t thread, void **retval){
  struct proc *curproc = myproc();
  struct proc *p;

  acquire(&ptable.lock);

  for(;;){
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if(!(p->pid == curproc->pid && p->tid == thread && p->state == ZOMBIE))
        continue;

      // Found one.
      kfree(p->kstack);
      p->kstack = 0;
      p->pid = 0;
      p->parent = 0;
      p->name[0] = 0;
      p->killed = 0;
      p->state = UNUSED;
      // thread 관련 설정 초기화
      p->memlimit = 0;
      p->stacksize = 0;
      p->mainthread = 0;
      p->sz = 0;
      p->base = 0;
      p->tid = 0;
      *retval = p->retval;
      p->retval = 0;
      release(&ptable.lock);
      return 0;
    }

    if(curproc->killed){
      cprintf("join killed\n");
      release(&ptable.lock);
      return -1;
    }

    sleep(curproc, &ptable.lock);
  }
}

// thread_create wrapper function
int sys_thread_create(void){
  int thread, start_routine, arg;
  if((argint(0, &thread) < 0) || (argint(1, &start_routine) < 0) || (argint(2, &arg) < 0))
	  return -1;
  
  return thread_create((thread_t*)thread, (void *)start_routine, (void *)arg);
}

// thread_exit wrapper function
int sys_thread_exit(void){
  int retval;

  if (argint(0, &retval) < 0) return -1;

  thread_exit((void *)retval);

  return 0;
}

// thread_join wrapper function
int sys_thread_join(void){
  int thread;
  int retval;

  if ((argint(0, &thread) < 0) || (argint(1, &retval) < 0)) return -1;

  return thread_join((thread_t)thread, (void **)retval);
}