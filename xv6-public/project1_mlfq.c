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

extern struct mlfqs schedmlfq;

// proc 셋팅
// level이 -1이면 0, priority로 입력
// -1이 아니면 다음 큐 이동으로 간주
void procwrapinit(struct proc_w *procwrap, struct proc *_proc, int level, int priority) {
    if(level == -1){
        procwrap->quelevel = 0;
        procwrap->priority = priority;
    }else{
        if(level == 2){
            procwrap->quelevel = level;
            procwrap->priority = priority == 0 ? 0 : (priority - 1);
        }else{
            procwrap->quelevel = level + 1;
            procwrap->priority = priority;
        }
    }
    procwrap->procptr = _proc;
    // 모든 proc은 schedmlfq에 push 될 때 timequantum을 0으로 초기화하고 들어감
    procwrap->timequantum = 0;
}

struct proc_w *findprocwrap(struct proc *_proc) {
    struct mlfq *qs = &schedmlfq.mlfql;
    struct mlfq *q = qs;
    struct queue *node;
    
    // 큐에서 proc 찾기
    for (; q < &qs[LEVELSIZE]; q++){
        // 큐의 head가 0이면(해당 레벨의 큐가 비었으면) 다음 레벨 큐 확인
        if (!q->head){
            continue;
        }
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
    struct queue *now = q->head, *prev = (void *)0;
    do{
        if(now->procwrap.procptr->state == RUNNABLE){
            if(now == q->head){
                if(now == q->tail)
                {
                    q->head = q->tail = 0;
                }else{
                    q->head = now->next;
                }
                now->isused = now->next = 0;
            }else if(now == q->tail){
                now->isused = now->next = 0;
                prev->next = 0;
                q->tail = prev;
            }else{
                prev->next = now->next;
                now->isused = now->next = 0;
            }
            return &now->procwrap;
        }else{
            prev = now;
            now = now->next;
        }
    }while(now);

    // 해당 큐에 RUNNABLE한 proc이 없으면 0 반환
    return 0;
}

// schedmlfq에서 스케줄 원칙에 따라 선택된 RUNNABLE한 proc_w 반환
// 해당 procwrap(queue node)는 schedmlfq에서 제거됨
struct proc_w *popproc() {
    struct mlfq *qs = &schedmlfq.mlfql;
    struct mlfq *q = qs;
    struct proc_w *popped;

    // schedmlfq에서 RUNNABLE한 proc 찾기
    for (; q < &qs[LEVELSIZE]; q++){
        // 큐의 head가 0이면(해당 레벨의 큐가 비었으면) 다음 레벨 큐 확인
        if (!q->head){
            continue;
        }
        if(popped = pop(q)){
            return popped;
        }
    }

    // 큐에 RUNNABLE한 proc이 하나도 없으면 panic emit
    panic("queue is empty, but pop");
}

// 특정 레벨 큐에 삽입하는 함수
// push 하기 전에 procwrap 세팅 필요(procwrap은 여기서 수정되지 않음)
// 성공하면 0 리턴
int push(struct proc_w *procwrap) {
    // priority를 고려한 level 설정
    int level = procwrap->quelevel == 2 ? procwrap->quelevel + procwrap->priority : procwrap->quelevel;

    struct mlfq *q = &schedmlfq.mlfql[level];
    struct queue *node = &q->procqueue;

    // 큐에서 사용하지 않는 노드 선택
    for (; node < &q->procqueue[QUESIZE] && node->isused; node++);

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
    if (q->head) {
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

void newproc(struct proc *_proc) {
    struct proc_w _procw;
    procwrapinit(&_procw, _proc, -1, 3);
    pushproc(&_procw);
}

// priority boosting
int boosting(){
    struct mlfq *qs = &schedmlfq.mlfql;
    struct mlfq *q = qs;
    struct proc_w *popped;
    struct queue *node;
    struct proc *temp[QUESIZE];
    struct proc_w _proc;
    int cnt = 0;

    for (; q < &qs[LEVELSIZE]; q++) {
        q->head = q->tail = 0;
        node = &q->procqueue;
        for (; node < &q->procqueue[QUESIZE]; node++){
            node->isused = 0;
            node->next = 0;
            node->procwrap.priority = node->procwrap.quelevel = node->procwrap.timequantum = 0;
            temp[cnt++] = node->procwrap.procptr;
        }
    }

    for (int i = 0; i < cnt; i++) {
        if(temp[i] == schedmlfq.nowproc)
            continue;
        newproc(temp[i]);
    }
    return 0;
}