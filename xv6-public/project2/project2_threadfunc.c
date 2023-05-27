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

// todo
// stacksize fork, exec 등에 추가하기(최소 1)

// fork, exec, sbrk, kill, sleep, pipe syscall 확인 또는 수정

// fork와 비슷한 방식으로 동작
int thread_create(thread_t *thread, void *(*start_rootine)(void *), void *arg){
    int i, idx = -1;
    uint sz;
    struct proc *np;
    struct proc *curproc = myproc();

    // Allocate process.
    if ((np = allocproc()) == 0) {
        return -1;
    }

    acquire(&ptable.lock);

    if(curproc->mainthread != 0){
        curproc = curproc->mainthread;
    }

    if(curproc->memlimit != 0 && curproc->memlimit < curproc->sz + PGSIZE){
        return -1;
    }
    
    for(i=0;i<MAXTHREAD;i++){
        if(curproc->tstack[i] == 0){
            idx = i;
            curproc->tstack[i] = 1;
            break;
        }
    }

    if(idx == -1){
        panic("need more memory");
        return -1;
    }

    // np의 thread 값 설정
    np->mainthread = curproc;
    np->tid = ++idx;
    *thread = np->tid;

    // np의 proc 값 설정
    np->pgdir = curproc->pgdir;
    *np->tf = *curproc->tf;
    np->tid = *thread;
    np->parent = curproc->parent;
    np->pid = curproc->pid;
    sz = curproc->sz - curproc->pgcnt * PGSIZE;

    if((np->sz = allocuvm(np->pgdir, sz + (idx - 1) * PGSIZE, sz + idx * PGSIZE)) == 0){
        curproc->tstack[idx - 1] = 0;
        np->state = UNUSED;
        release(&ptable.lock);
        return -1;
    }

    safestrcpy(np->name, curproc->name, sizeof(curproc->name));

    // np의 stack, tf 설정
    uint ustack[2];
    ustack[0] = 0xffffffff;
    ustack[1] = (uint)arg;

    if (copyout(np->pgdir, np->sz - 8, ustack, 8) < 0) {
        deallocuvm(np->pgdir, sz + idx * PGSIZE, sz + (idx - 1) * PGSIZE);
        curproc->tstack[idx - 1] = 0;
        np->state = UNUSED;
        release(&ptable.lock);
        return -1;
    }

    curproc->pgcnt += 1;
    curproc->sz += PGSIZE;
    curproc->tid += 1;
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
    uint sz;

    acquire(&ptable.lock);

    for(;;){
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if(!(p->pid == curproc->pid && p->tid == thread && p->state == ZOMBIE))
                continue;

            // Found one.
            kfree(p->kstack);
            p->kstack = 0;
            sz = curproc->sz - curproc->pgcnt * PGSIZE;
            deallocuvm(p->pgdir, sz + p->tid * PGSIZE, sz + (p->tid - 1) * PGSIZE);
            p->pid = 0;
            p->parent = 0;
            p->name[0] = 0;
            p->killed = 0;
            p->state = UNUSED;
            // thread 관련 설정 초기화
            p->memlimit = 0;
            p->stacksize = 0;
            p->mainthread = 0;
            p->pgcnt = 0;
            curproc->tstack[p->tid - 1] = 0;
            curproc->pgcnt -= 1;
            curproc->sz -= PGSIZE;
            p->tid = 0;
            *retval = p->retval;
            p->retval = 0;
            release(&ptable.lock);
            return 0;
        }

        if(curproc->killed){
            release(&ptable.lock);
            return -1;
        }

        sleep(curproc, &ptable.lock);
    }
}

int sys_thread_create(void){
    int thread, start_routine, arg;
    if((argint(0, &thread) < 0) || (argint(1, &start_routine) < 0) || (argint(2, &arg) < 0))
	    return -1;
    
    return thread_create((thread_t*)thread, (void *)start_routine, (void *)arg);
}

int sys_thread_exit(void){
    int retval;

    if (argint(0, &retval) < 0) return -1;

    thread_exit((void *)retval);

    return 0;
}

int sys_thread_join(void){
    int thread;
    int retval;

    if ((argint(0, &thread) < 0) || (argint(1, &retval) < 0)) return -1;

    return thread_join((thread_t)thread, (void **)retval);
}