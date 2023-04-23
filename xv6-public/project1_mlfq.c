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

// proc 셋팅
// 0이면 설정한대로, 1이면 큐 이동 (set Priority 때문에 이렇게 해야함)
// sche로 인해 중간에 바뀔 수 있으므로 timequantum 남겨둬야함
void
procwrapinit(struct proc_w *procwrap, struct proc *_proc, int level, int priority, int timequantum, int isset)
{
  if(isset){
    procwrap->quelevel = level;
    procwrap->priority = priority;
    procwrap->timequantum = timequantum;
  }else{
    // level이 2이면 우선순위 증가(priority 값 감소)
    if(level == 2){
      procwrap->quelevel = level;
      procwrap->priority = priority == 0 ? 0 : (priority - 1);
    }else{
      // level이 2가 아니면 다음 레벨의 큐로 이동
      procwrap->quelevel = level + 1;
      procwrap->priority = priority;
    }
    // 해당하는 레벨에 맞는 timequantum 설정
    procwrap->timequantum = procwrap->quelevel * 2 + 4;
  }
  procwrap->procptr = _proc;
}

// 해당 큐에서 헤드 제거하고 리턴
struct proc_w*
pop(struct mlfq *q)
{
  struct queue *now = q->head, *prev = (void *)0;
  do{
    // RUNNABLE이면
    if(now->procwrap.procptr->state == RUNNABLE){
      // head에서 제거 처리
      if(now == q->head){
        if(now == q->tail){
          q->head = q->tail = 0;
        }else{
          q->head = now->next;
        }
        now->isused = 0;
        now->next = 0;
      }else if(now == q->tail){
        // tail에서 제거 처리
        now->isused = 0;
        now->next = 0;
        prev->next = 0;
        q->tail = prev;
      }else{
        // head와 tail사이에서 제거 처리
        prev->next = now->next;
        now->isused = 0;
        now->next = 0;
      }
      // MLFQ에서 사용하지 않음을 프로세스에 체크
      now->procwrap.procptr->isinmlfq = 0;
      // proc_w 주소 반환
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
struct proc_w*
popproc()
{
  struct mlfq *qs = schedmlfq.mlfql;
  struct mlfq *q = qs;
  struct proc_w *popped = 0;

  // schedmlfq에서 RUNNABLE한 proc 찾기
  for (; q < &qs[LEVELSIZE]; q++){
    // 큐의 head가 0이면(해당 레벨의 큐가 비었으면) 다음 레벨 큐 확인
    if (!q->head){
      continue;
    }
    if((popped = pop(q))){
      return popped;
    }
  }
  return 0;
}

// RUNNABLE이나 SLEEPING이 아니면 제거
int
statepop(struct mlfq *q)
{
  struct queue *now = q->head, *prev = (void *)0;
  int cnt = 0;
  do {
    // RUNNABLE이나 SLEEPING이 아니면
    if (now->procwrap.procptr->state != RUNNABLE && now->procwrap.procptr->state != SLEEPING) {
      // now가 head인 경우
      if (now == q->head) {
        // head와 tail 처리
        if (now == q->tail) {
          q->head = q->tail = 0;
        } else {
          q->head = now->next;
        }
        now->isused = 0;
        now->next = 0;
        now = q->head;
      } else if (now == q->tail) {
        // now가 tail인 경우 tail 처리
        now->isused = 0;
        now->next = 0;
        prev->next = 0;
        q->tail = prev;
        now = 0;
      } else {
        // now가 head와 tail 사이인 경우 처리
        prev->next = now->next;
        now->isused = 0;
        now->next = 0;
        now = prev->next;
      }
      // MLFQ에서 사용하지 않음을 프로세스에 표시
      now->procwrap.procptr->isinmlfq = 0;
      cnt++;
    } else {
      // 조건에 불일치하면 다음 노드 확인
      prev = now;
      now = now->next;
    }
  } while (now); // 해당 레벨의 큐를 모두 확인할 때까지 반복

  return cnt;
}

// schedmlfq 안에 있는 zombie, unused 등 사용하지 않는 노드 제거
// 제거한 노드 개수 반환
int
clearmlfq()
{
  struct mlfq *qs = schedmlfq.mlfql;
  struct mlfq *q = qs;
  int cnt = 0;
  for (; q < &qs[LEVELSIZE]; q++) {
    if(q->head)
      cnt += statepop(q);
  }
  return cnt;
}

// 특정 레벨 큐에 삽입하는 함수
// push 하기 전에 procwrap 세팅 필요(procwrap은 여기서 수정되지 않음)
// 성공하면 0 리턴
int
push(struct proc_w *procwrap)
{
  // 이미 큐 안에 있으면 push하지 않음
  if(procwrap->procptr->isinmlfq){
    return 1;
  }
  // priority를 고려한 level 설정
  int level = procwrap->quelevel == 2 ? (procwrap->quelevel + procwrap->priority) : (procwrap->quelevel);

  struct mlfq *q = &schedmlfq.mlfql[level];
  struct queue *node = q->procqueue;

  // 큐에서 사용하지 않는 노드 선택
  for (; node < &q->procqueue[QUESIZE] && node->isused; node++);

  // 사용하지 않는 노드 없으면 panic emit
  if (node == &q->procqueue[QUESIZE]){
    panic("queue is full, but push");
  }
  // node에 proc 매칭
  node->procwrap.procptr = procwrap->procptr;
  node->procwrap.priority = procwrap->priority;
  node->procwrap.quelevel = procwrap->quelevel;
  node->procwrap.timequantum = procwrap->timequantum;
  node->procwrap.procptr->isinmlfq = 1;
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

// schedmlfq L0의 헤드에 삽입
int
headpush(struct proc_w *procwrap)
{
  // 이미 큐 안에 있으면 push하지 않음
  if (procwrap->procptr->isinmlfq) {
    return 1;
  }
  struct mlfq *q = &schedmlfq.mlfql[0];
  struct queue *node = q->procqueue;

  // 큐에서 사용하지 않는 노드 선택
  for (; node < &q->procqueue[QUESIZE] && node->isused; node++);

  // 사용하지 않는 노드 없으면 panic emit
  if (node == &((q->procqueue)[QUESIZE])) {
    panic("queue is full, but headpush");
  }

  // node에 proc 매칭
  node->procwrap.procptr = procwrap->procptr;
  node->procwrap.priority = procwrap->priority;
  node->procwrap.quelevel = procwrap->quelevel;
  node->procwrap.timequantum = procwrap->timequantum;
  node->procwrap.procptr->isinmlfq = 1;
  node->isused = 1;

  if(q->head){
    node->next = q->head;
    q->head = node;
  }else{
    node->next = 0;
    q->head = q->tail = node;
  }

  return 0;
}

// mlfq 전체에 규칙에 맞게 삽입하기 위한 함수
// 성공하면 0 리턴
int
pushproc(struct proc_w *procwrap)
{
  return push(procwrap);
}

// allocproc 같이 새로운 프로세스가 추가될 때 사용
void
newproc(struct proc *_proc)
{
  struct proc_w _procw;
  // 사용하지 않는 노드 제거
  clearmlfq();
  // MLFQ에 넣을 노드 셋팅
  procwrapinit(&_procw, _proc, 0, 3, 4, 1);
  // 노드 삽입
  pushproc(&_procw);
}

// priority boosting
int
boosting()
{
  struct mlfq *qs = schedmlfq.mlfql;
  struct mlfq *q = qs;
  struct queue *node, *now;
  struct proc *temp[QUESIZE];
  struct proc_w _proc;
  int cnt = 0;
  //cprintf("boosting\n");

  // RUNNABLE, SLEEPING 제외하고 전부 제거
  clearmlfq();
  // MLFQ 전부 초기화하면서 큐 순서대로 temp에 저장
  for (; q < &qs[LEVELSIZE]; q++) {
    if(!q->head)
      continue;
    node = q->head;
    do{
      temp[cnt++] = node->procwrap.procptr;
      node->procwrap.procptr->isinmlfq = 0;
      node->isused = 0;
      node->procwrap.procptr = 0;
      node->procwrap.priority = node->procwrap.quelevel = node->procwrap.timequantum = 0;
      now = node->next;
      node->next = 0;
      node = now;
    }while(node);
    q->head = q->tail = 0;
  }

  // lock된 프로세스가 있다면, 그것만 제외하고 전부 L0에 삽입
  for (int i = 0; i < cnt; i++) {
    if(schedmlfq.islock && temp[i] == schedmlfq.nowproc)
      continue;
    procwrapinit(&_proc, temp[i], 0, 3, 4, 1);
    pushproc(&_proc);
  }

  // 만약 lock된 상태였으면 L0 큐의 헤더로 삽입
  if(schedmlfq.islock){
    procwrapinit(&_proc, schedmlfq.nowproc, 0, 3, 4, 1);
    headpush(&_proc);
  }

  // boosting 했으면 lock 해제
  schedmlfq.islock = 0;
  schedmlfq.lockproc = 0;
  // global tick 초기화
  schedmlfq.ticks = 1;
  return 0;
}