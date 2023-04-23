#include "project_define.h"
#include "proc.h"

struct proc_w {
    struct proc *procptr;            // 가리키고있는 procptr
    int priority;                    // Process 우선순위, 초기값 3, 값이 작을수록 우선순위 높음
    int quelevel;                    // Process가 위치한 대기 큐 레벨
    int timequantum;                 // Process가 실행된 만큼의 tick
};

struct queue {
  struct queue *next;                // 다음 노드(사용중인 노드끼리만 가리킴)
  struct proc_w procwrap;            // 가리키고 있는 proc의 wrap
  int isused;                        // 사용중인 노드인지 여부
};

struct mlfq {
  struct queue procqueue[QUESIZE];   // 큐의 레벨별 크기
  struct queue *head;                // 큐의 헤드
  struct queue *tail;                // 큐의 테일
};

struct mlfqs {
  // LEVELSIZE = LEVEL + PRIORITY
  // PRIORITY 만큼의 큐를 더 만들어서 같은 우선순위끼리 FCFS가 가능하도록 한다.
  struct mlfq mlfql[LEVELSIZE];
  struct proc *nowproc;
  struct proc *lockproc; // lockproc 확인용
  int priority; // process의 priority
  int quelevel; // process의 que level
  int timequantum; // process의 남은 timequantum
  int islock; // 현재 scheduler lock이 수행중인지
  int isTimeinterrupt; // time interrupt가 발생해서 스케줄링 하는지 확인
  uint ticks; // global tick
};