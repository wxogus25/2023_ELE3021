#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "project_define.h"
#include "project1_mlfq.h"

extern struct mlfq *mlfqs;

// proc 셋팅
void procwrapinit(struct proc_w *procwrap, struct proc *_proc, int level, int priority) {
    procwrap->procptr = _proc;
    procwrap->priority = priority;
    procwrap->quelevel = level;
    // 모든 proc은 mlfq에 push 될 때 timequantum을 0으로 초기화하고 들어감
    procwrap->timequantum = 0;
}

struct proc_w *findprocwrap(struct proc *_proc) {
    struct mlfq *q = mlfqs;
    struct queue *node;
    // 큐의 head가 0이면(해당 레벨의 큐가 비었으면) 다음 레벨 큐 확인
    for (; q < &mlfqs[LEVELSIZE] && !q->head; q++){
        node = &q->head;
        do {
            if(node->procwrap.procptr == _proc){
                return &node->procwrap;
            }
        } while (node->next != 0);
    }

    // 사용되고 있는 노드 중 찾는 proc이 없으면 panic emit
    panic("proc not found in mlfqs");
}

// 해당 큐에서 헤드 제거하고 리턴
struct proc_w *pop(struct mlfq *q) {
    struct queue *tpque = q->head;
    if (q->head->next){
        q->head = q->head->next;
    } else {
        q->head = q->tail = 0;
    }

    // 큐의 노드가 있던 위치를 초기화하고 procwrap 반환
    tpque->isused = tpque->next = 0;
    return &tpque->procwrap;
}

// mlfq에서 스케줄 원칙에 따라 선택된 proc 반환
// 해당 procwrap(queue node)는 mlfq에서 제거됨
struct proc *popproc() {
    struct mlfq *q = mlfqs;
    // 큐의 head가 0이면(해당 레벨의 큐가 비었으면) 다음 레벨 큐 확인
    for (; q < &mlfqs[LEVELSIZE] && !q->head; q++);

    // q가 존재하면 맨 앞에 있는 노드 제거하고 반환
    if (q->head){
        return pop(q)->procptr;
    }

    // 큐에 proc이 하나도 없으면 panic emit
    panic("queue is empty, but pop");
}

// 특정 레벨 큐에 삽입하는 함수
// push 하기 전에 procwrap 세팅 필요(procwrap은 여기서 수정되지 않음)
// 성공하면 0 리턴
int push(struct proc_w *procwrap) {
    // priority를 고려한 level 설정
    int level = procwrap->quelevel == 2 ? procwrap->quelevel + procwrap->priority : procwrap->quelevel;

    struct mlfq *q = &mlfqs[level];
    struct queue *node = &q->procqueue;

    // 큐에서 사용하지 않는 노드 선택
    for (; node < &((q->procqueue)[QUESIZE]) && node->isused; node++);

    // 사용하지 않는 노드 없으면 panic emit
    if (node == &((q->procqueue)[QUESIZE])){
        panic("queue is full, but push");
    }

    // node에 proc 매칭
    node->procwrap.procptr = procwrap->procptr;
    node->procwrap.priority = procwrap->priority;
    node->procwrap.quelevel = procwrap->quelevel;
    node->procwrap.timequantum = procwrap->timequantum;
    node->next = 0;
    node->isused = 1;

    // 큐가 비어있지 않으면 level에 큐 삽입
    if (q->tail) {
        q->tail->next = node;
        q->tail = node;
    } else {  // 큐가 비어있으면
        q->tail = q->head = node;
    }

    return 0;
}

// mlfq 전체에 규칙에 맞게 삽입하기 위한 함수
// 성공하면 0 리턴
int pushproc(struct proc_w *procwrap) {
    return push(procwrap);
}