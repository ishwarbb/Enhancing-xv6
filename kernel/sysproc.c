#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_waitx(void)
{
  uint64 addr, addr1, addr2;
  uint wtime, rtime;
  argaddr(0, &addr);
  argaddr(1, &addr1); // user virtual memory
  argaddr(2, &addr2);
  int ret = waitx(addr, &wtime, &rtime);
  struct proc* p = myproc();
  if (copyout(p->pagetable, addr1,(char*)&wtime, sizeof(int)) < 0)
    return -1;
  if (copyout(p->pagetable, addr2,(char*)&rtime, sizeof(int)) < 0)
    return -1;
  return ret;
}


uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


uint64
sys_trace(void)
{
  // printf("setting work\n");
  // printf("myproc()->Trace_Mask = %d\n",myproc()->Trace_Mask);
  // printf("myproc()->Is_Traced = %d\n",myproc()->Is_Traced);
  // myproc()->Trace_Mask = *mask;
  argint(0,&myproc()->Trace_Mask);
  printf("setting TM = %d \n",myproc()->Trace_Mask);
  myproc()->Is_Traced = 1;
  // printf(" new myproc()->Trace_Mask = %d\n",myproc()->Trace_Mask);
  // printf(" new myproc()->Is_Traced = %d\n",myproc()->Is_Traced);
  return 0;
}

uint64
sys_sigalarm(void)
{
  int xticks;
  uint64 addr;

  argint(0, &xticks);
  argaddr(1, &addr);

  // printf("\n>> xticks: %d\n", xticks);

  // return sigalarm(xticks, addr);

  if (xticks < 0) return -1;
  if (addr < 0) return -1;

  myproc()->ticks = xticks;
  myproc()->handler = addr;

  myproc()->current_ticks = 0;
  return 0;
}

uint64
sys_sigreturn(void)
{
  // return sigreturn();
  struct proc* p = myproc();

  memmove(p->trapframe, p->sigalarm_tf, PGSIZE);
  kfree(p->sigalarm_tf);
  p->sigalarm_tf = 0;

  p->current_ticks = 0;
  return p->trapframe->a0;
}

uint64
sys_settickets(void)
{
  #ifdef LBS
  int xtickets;
  argint(0, &xtickets);

  return settickets(xtickets);

  #else
  printf("This is not Lottery Based Scheduling.\n");
  #endif

  return 0;
}

uint64
sys_setpriority(void)
{  
  int new_priority;
  int pid;

  argint(0, &new_priority);
  argint(1, &pid);

  int ret = setpriority(new_priority, pid);

  if (ret != -1 && new_priority < ret)
  {
    yield();
  }
  return ret;
}
