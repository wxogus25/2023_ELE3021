#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"
#include "spinlock.h"
#include "project2_thread.h"


// 하나의 프로세스에서 나온 스레드는 같은 pid 가짐
// 대신 서로 다른 tid 가짐


// todo : proc에 thread id 관련 셋팅
// fork, exec, sbrk, kill, sleep, pipe syscall 확인 또는 수정

int thread_create(thread_t *thread, void *(*start_rootine)(void *), void *arg);
void thread_exit(void *retval);
int thread_join(thread_t *thread, void **retval);
