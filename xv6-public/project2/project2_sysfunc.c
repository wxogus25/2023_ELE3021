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

// exec2 system call
// stacksize는 1 이상 100 이하
// 실패하면 return -1
int exec2(char *path, char **argv, int stacksize){
    if(stacksize < 1 || stacksize > 100){
        return -1;
    }
    char *s, *last;
    int i, off;
    uint argc, sz, sp, ustack[3 + MAXARG + 1];
    struct elfhdr elf;
    struct inode *ip;
    struct proghdr ph;
    pde_t *pgdir, *oldpgdir;
    struct proc *curproc = myproc();
    struct proc *p;

    begin_op();

    if ((ip = namei(path)) == 0) {
        end_op();
        cprintf("exec: fail\n");
        return -1;
    }
    ilock(ip);
    pgdir = 0;

    // Check ELF header
    if (readi(ip, (char *)&elf, 0, sizeof(elf)) != sizeof(elf)) goto bad;
    if (elf.magic != ELF_MAGIC) goto bad;

    if ((pgdir = setupkvm()) == 0) goto bad;

    // Load program into memory.
    sz = 0;
    for (i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph)) {
        if (readi(ip, (char *)&ph, off, sizeof(ph)) != sizeof(ph)) goto bad;
        if (ph.type != ELF_PROG_LOAD) continue;
        if (ph.memsz < ph.filesz) goto bad;
        if (ph.vaddr + ph.memsz < ph.vaddr) goto bad;
        if ((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0) goto bad;
        if (ph.vaddr % PGSIZE != 0) goto bad;
        if (loaduvm(pgdir, (char *)ph.vaddr, ip, ph.off, ph.filesz) < 0)
            goto bad;
    }
    iunlockput(ip);
    end_op();
    ip = 0;

    // Allocate two pages at the next page boundary.
    // Make the first inaccessible.  Use the second as the user stack.
    sz = PGROUNDUP(sz);

    // sz + (1 + stacksize) * PGSIZE가 memory limit를 넘는지 확인하고, 넘으면 bad로 이동
    if (curproc->sz != 0 && sz + (1 + stacksize) * PGSIZE > curproc->sz) goto bad;
    // 넘지 않으면 메모리 할당
    if ((sz = allocuvm(pgdir, sz, sz + (1 + stacksize) * PGSIZE)) == 0) goto bad;
    clearpteu(pgdir, (char *)(sz - (1 + stacksize) * PGSIZE));
    sp = sz;

    // Push argument strings, prepare rest of stack in ustack.
    for (argc = 0; argv[argc]; argc++) {
        if (argc >= MAXARG) goto bad;
        sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
        if (copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
            goto bad;
        ustack[3 + argc] = sp;
    }
    ustack[3 + argc] = 0;

    ustack[0] = 0xffffffff;  // fake return PC
    ustack[1] = argc;
    ustack[2] = sp - (argc + 1) * 4;  // argv pointer

    sp -= (3 + argc + 1) * 4;
    if (copyout(pgdir, sp, ustack, (3 + argc + 1) * 4) < 0) goto bad;

    // Save program name for debugging.
    for (last = s = path; *s; s++)
        if (*s == '/') last = s + 1;
    safestrcpy(curproc->name, last, sizeof(curproc->name));

    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->pid == curproc->pid && p != curproc) {
            p->state = ZOMBIE;
            if (p->mainthread == 0) {
                for (int fd = 0; fd < NOFILE; fd++) {
                    p->ofile[fd] = 0;
                }
                p->cwd = 0;
            }
        }
    }
    release(&ptable.lock);

    // Commit to the user image.
    oldpgdir = curproc->pgdir;
    if (curproc->mainthread == 0) {
        freevm(oldpgdir);
    }
    curproc->pgdir = pgdir;
    curproc->sz = sz;
    curproc->tf->eip = elf.entry;  // main
    curproc->tf->esp = sp;
    curproc->tid = 0;
    curproc->mainthread = 0;
    curproc->retval = 0;
    switchuvm(curproc);
    return 0;

bad:
    // pgdir free하면서 page table에 연결된 메모리도 같이 할당 해제함
    if (pgdir) freevm(pgdir);
    if (ip) {
        iunlockput(ip);
        end_op();
    }
    return -1;
}

// exec2 wrapper function
int sys_exec2(void) {
    char *path, *argv[MAXARG];
    int i;
    int stacksize;
    uint uargv, uarg;

    if (argstr(0, &path) < 0 || argint(1, (int *)&uargv) < 0 || argint(2, (int*)&stacksize) < 0) {
        return -1;
    }
    memset(argv, 0, sizeof(argv));
    for (i = 0;; i++) {
        if (i >= NELEM(argv)) return -1;
        if (fetchint(uargv + 4 * i, (int *)&uarg) < 0) return -1;
        if (uarg == 0) {
            argv[i] = 0;
            break;
        }
        if (fetchstr(uarg, &argv[i]) < 0) return -1;
    }
    return exec2(path, argv, stacksize);
}

// 해당 pid를 가진 프로세스의 memory limit을 limit으로 변경
int setmemorylimit(int pid, int limit){
    struct proc *p;
    // 해당하는 pid를 가진 프로세스를 찾고 변경했는지 확인하는 용도
    int flag = 1;
    // limit이 0보다 작거나 pid가 0 이하이면 return -1
    if(limit < 0 || pid <= 0)
        return -1;
    // 해당하는 pid를 가진 프로세스를 찾으면 limit 변경
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        // pid가 0이 아니라면 존재하는 프로세스이므로 변경
        // zombie이거나 kill 됐어도 아직 free 안됐으면 변경함
        if (p->pid == pid && p->mainthread == 0){
            // limit이 0 보다 크면 sz와 비교
            if(limit > 0){
                // mainthread의 sz만 의미가 있으므로 비교
                if(limit >= p->sz){
                    flag = 0;
                    p->memlimit = limit;
                }else
                // 때문에 sz가 limit보다 크면 pid가 같은 다른 프로세스들의
                // memlimit을 다시 돌릴 필요없이 바로 종료해도 됨
                    return -1;
            // limit이 0이면 memlimit 바로 변경
            }else{
                flag = 0;
                p->memlimit = 0;
            }
        }
    // flag가 1이면 해당 pid를 가진 프로세스가 없으므로 return -1
    // flag가 0이면 해당 pid를 가진 프로세스가 있어서 변경한 것이므로 return 0
    return flag * -1;
}

// setmemorylimit wrapper function
int sys_setmemorylimit(void){
    int pid, limit;

    if (argint(0, (int*)&pid) < 0 || argint(1, (int*)&limit) < 0)
        return -1;

    return setmemorylimit(pid, limit);
}

// 현재 state가 RUNNABLE, RUNNING, SLEEPING인 프로세스들의 정보 출력
// [프로세스 이름, pid, 스텍용 페이지 개수, 할당받은 메모리 크기, 메모리 최대 제한] 순서대로 출력
int pslist(void){
    struct proc *p;
    int flag = 1;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->state == RUNNABLE || p->state == RUNNING || p->state == SLEEPING){
            if(p->mainthread == 0){
                // sz는 uint지만 int 범위 내에서 양수로 존재한다고 가정하고 출력
                cprintf("%s %d %d %d %d\n", p->name, p->pid, p->stacksize, p->sz, p->memlimit);
                flag = 0;
            }
        }
    }
    // 아무것도 출력 안하면 return -1, 아니면 return 0
    return flag * -1;
}

// pslist wrapper function
int sys_pslist(void){
    return pslist();
}