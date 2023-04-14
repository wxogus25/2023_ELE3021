#include "project_define.h"
#include "proc.h"

struct procwrapper {
    struct proc *procptr;            // 가리키고있는 proc
    int priority;                    // Process 우선순위, 초기값 3, 값이 작을수록 우선순위 높음
    int quelevel;                    // Process가 위치한 대기 큐 레벨
    int timequantum;                 // Process가 실행된 만큼의 tick
};

struct queue {
  struct queue *next;                // 다음 노드(사용중인 노드끼리만 가리킴)
  struct procwrapper procwrap;       // 가리키고 있는 proc의 wrap
  int isused;                        // 사용중인 노드인지 여부
};

struct mlfq {
  struct queue procqueue[QUESIZE];   // 큐의 레벨별 크기
  struct queue *head;                // 큐의 헤드
  struct queue *tail;                // 큐의 테일
  int level;                         // 큐의 레벨
};
