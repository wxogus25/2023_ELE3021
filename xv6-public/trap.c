#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "project1_mlfq.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;
extern struct mlfqs schedmlfq;

extern int sys_schedulerLock(void);
extern int sys_schedulerUnlock(void);

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);

  // user mode로 수행 가능한 128번 interrupt 추가
  SETGATE(idt[128], 1, SEG_KCODE<<3, vectors[128], DPL_USER);
  // scheduler lock
  SETGATE(idt[129], 1, SEG_KCODE<<3, vectors[128], DPL_USER);
  // scheduler unlock
  SETGATE(idt[130], 1, SEG_KCODE<<3, vectors[128], DPL_USER);

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  // 128번 interrupt handling
  if(tf->trapno == 128){
    if (myproc()->killed)
      exit();
    myproc()->tf = tf;
    // 커널 코드 직접 실행하게 함
    mycall();
    myproc()->tf->eax = 0;
    if (myproc()->killed)
      exit();
    return;
  }

  // 129번 scheduler lock
  if (tf->trapno == 129) {
    if (myproc()->killed) exit();
    myproc()->tf = tf;
    myproc()->tf->eax = sys_schedulerLock();
    if (myproc()->killed) exit();
    return;
  }

  // 130번 scheduler unlock
  if (tf->trapno == 130) {
    if (myproc()->killed) exit();
    myproc()->tf = tf;
    myproc()->tf->eax = sys_schedulerUnlock();
    if (myproc()->killed) exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi(); // end of interrupt CPU에게 알림(타이머 인터럽트?)
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING && tf->trapno == T_IRQ0+IRQ_TIMER){
    schedmlfq.ticks++;
    // global ticks이 100의 배수거나, 현재 schedulerLock이 실행된게 아니면
    if(schedmlfq.ticks % 100 == 0 || schedmlfq.islock < 1)
      // 스케줄링 시작
      yield();
    else // lock 걸렸으면 pass
      schedmlfq.timequantum--;
  }
    

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
