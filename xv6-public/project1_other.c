#include "types.h"
#include "defs.h"

// 다음 프로세스에세 프로세서 양보
void myYield(void) {}

// 프로세스가 속한 큐의 레벨 반환
int getLevel(void) {}

// 해당 pid의 프로세스의 priority 설정
void setPriority(int pid, int priority) {}