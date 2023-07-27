#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define DATA 0

int min(int a, int b)
{
  return (a < b) ? a : b;
}
int max(int a, int b)
{
  return (a > b) ? a : b;
}

// PRNG code obtained from an online resource
unsigned short lfsr = 0xACE1u;
unsigned bit;

unsigned rand()
{
  bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
  return lfsr =  (lfsr >> 1) | (bit << 15);
}



struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

#define QSIZE 20000

struct proc *q0[QSIZE];
struct proc *q1[QSIZE];
struct proc *q2[QSIZE];
struct proc *q3[QSIZE];
struct proc *q4[QSIZE];
int e0 = 0;
int e1 = 0;
int e2 = 0;
int e3 = 0;
int e4 = 0;

int AGING_THRESHOLD[5] = {2,4,9,17,20};
// #define AGING_THRESHOLD 20

#define debug 0

void qpush_front(struct proc *q[], struct proc *p, int total)
{
  for (int i = total; i > 0; i--)
  {
    q[i] = q[i - 1];
  }
  q[0] = p;

  return;
}

int k = 0;

void qpush_back(struct proc *q[], struct proc *p, int total)
{
  q[total] = p;
  // printf("done %d\n",k++);

  return;
}

void qpop(struct proc *q[], int i, int total)
{
  for (int j = i; j < (total - 1); j++)
  {
    q[i] = q[i + 1];
  }
  // THe caller can decrement thr total

  return;
}

void qpop_and_push_back(struct proc *q[], int i, int total)
{
  struct proc *temp = q[i];
  for (int j = i; j < (total - 1); j++)
  {
    q[i] = q[i + 1];
  }
  q[total - 1] = temp;

  return;
}

void qfindnrem(struct proc* q[], struct proc* p, int total)
{
  for(int i = 0 ; i < total ; i++)
  {
    if(q[i] == p) qpop(q,i,total);
  }

  return;
}

int time_Slice[5] = {1, 2, 4, 8, 16};

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;
  
  for(p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if(pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int) (p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu*
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc*
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int
allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if(p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // for FCFS scheduling & PBS 
  p->creation_time = ticks;
  p->run_time = 0;
  p->sleep_time = 0;
  p->times_scheduled = 0;
  p->static_priority = 60;

  // Lottery Based Scheduling
  p->tickets = 1;

  //MLFQ
    qpush_back(q0,p,e0);
    e0++;
    p->mticks = 0;
    p->queue_no = 0;
    p->mstart = ticks;

          struct proc* curproc = p;
          if (DATA)
          {
            printf("%d %d %d\n", ticks, curproc->pid, curproc->queue_no);
          }

  // Allocate a trapframe page.
  if((p->trapframe = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;
  p->rtime = 0;
  p->etime = 0;
  p->ctime = ticks;

  
  p->Is_Traced = 0;
  p->Trace_Mask = 0;

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if(mappages(pagetable, TRAMPOLINE, PGSIZE,
              (uint64)trampoline, PTE_R | PTE_X) < 0){
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mappages(pagetable, TRAPFRAME, PGSIZE,
              (uint64)(p->trapframe), PTE_R | PTE_W) < 0){
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if(n > 0){
    if((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0) {
      return -1;
    }
  } else if(n < 0){
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // #ifdef LBS
  // copy tickets (Lottery Based Scheduling)
  np->tickets = p->tickets;
  // #endif

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  np->Trace_Mask = p->Trace_Mask;
  np->Is_Traced = p->Is_Traced;

  // increment reference counts on open file descriptors.
  for(i = 0; i < NOFILE; i++)
    if(p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  // printf("fork done\n");

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for(pp = proc; pp < &proc[NPROC]; pp++){
    if(pp->parent == p){
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status)
{
  struct proc *p = myproc();
  p->finish_time = ticks;

  if(p == initproc)
    panic("init exiting");

  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;
    p->etime = ticks;

  release(&wait_lock);
  struct proc* curproc = p;
  if (DATA)
  {
    printf("%d %d %d\n", ticks, curproc->pid, curproc->queue_no);
  }

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(pp = proc; pp < &proc[NPROC]; pp++){
      if(pp->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if(pp->state == ZOMBIE){
          // Found one.
          pid = pp->pid;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || killed(p)){
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

int CheckPresent(struct proc* p)
{
  for(int i = 0 ; i < e0; i++)
  {
    if(p == q0[i]) return 1;
  }
  for(int i = 0 ; i < e1; i++)
  {
    if(p == q1[i]) return 1;
  }
  for(int i = 0 ; i < e2; i++)
  {
    if(p == q2[i]) return 1;
  }
  for(int i = 0 ; i < e3; i++)
  {
    if(p == q3[i]) return 1;
  }
  for(int i = 0 ; i < e4; i++)
  {
    if(p == q4[i]) return 1;
  }
  return 0;
}

void CheckRunnable()
{
  struct proc *p;
  // if(debug) printf("..");
  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if ((p->state == RUNNABLE) && (CheckPresent(p) == 0))
    {
      if(p->queue_no == 0)
      {
        qpush_front(q0,p,e0);
        if(debug)printf("adding %s\n",p->name);
        e0++;
      }
      if(p->queue_no == 1)
      {
        qpush_front(q1,p,e1);
        if(debug)printf("adding %s\n",p->name);
        e1++;
      }
      if(p->queue_no == 2)
      {
        qpush_front(q2,p,e2);
        if(debug)printf("adding %s\n",p->name);
        e2++;
      }
      if(p->queue_no == 3)
      {
        qpush_front(q3,p,e3);
        if(debug)printf("adding %s\n",p->name);
        e3++;
      }
      if(p->queue_no == 4)
      {
        qpush_front(q4,p,e4);
        if(debug)printf("adding %s\n",p->name);
        e4++;
      }
      release(&p->lock);
    }
    else
    {
      release(&p->lock);
    }
  }
}

void CheckAging()
{
  struct proc *p;
  // if(debug) printf("..");
  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if ((p->state == RUNNABLE) && (CheckPresent(p) == 1))
    {
      if(ticks - p->mstart >= AGING_THRESHOLD[p->queue_no])
      {
        if(p->queue_no == 0)
        {
          release(&p->lock);
          continue;
        }
        p->mstart = ticks;
        if(p->queue_no == 4) qpush_back(q3,p,e3);
        if(p->queue_no == 3) qpush_back(q2,p,e2);
        if(p->queue_no == 2) qpush_back(q1,p,e1);
        if(p->queue_no == 1) qpush_back(q0,p,e0);

        if(p->queue_no == 4) qfindnrem(q4,p,e4);
        if(p->queue_no == 3) qfindnrem(q3,p,e3);
        if(p->queue_no == 2) qfindnrem(q2,p,e2);
        if(p->queue_no == 1) qfindnrem(q1,p,e1);

          struct proc* curproc = p;
          if (DATA)
          {
            printf("%d %d %d\n", ticks, curproc->pid, curproc->queue_no);
          }
      }
    }
    release(&p->lock);
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  #ifdef RR
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);

      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
        release(&p->lock);
    }
  }
      #elif FCFS
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    struct proc* nextP = 0; // holds process to run next
    for(p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);

      // find the runnable process with the least start time
      if (p->state == RUNNABLE)
      {
        if (!nextP)
          nextP = p;

        else
          nextP = (nextP->creation_time > p->creation_time) ? p : nextP;
      }
      release(&p->lock);
    }

    // if a process was found, run it
    if (nextP) 
    {
      p = nextP;
      acquire(&p->lock);

      if(p->state == RUNNABLE) {
      // Switch to chosen process.  It is the process's job
      // to release its lock and then reacquire it
      // before jumping back to us.
      p->state = RUNNING;
      c->proc = p;
      swtch(&c->context, &p->context);

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      }
      release(&p->lock);
    }
  }

  #elif LBS

  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;)
  {
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();


    int total_tickets = 0;
    for(p = proc; p < &proc[NPROC]; p++)
    {
      if (p->state == RUNNABLE)
        total_tickets += p->tickets;
    }

    int winner = rand() % total_tickets;

    int ticket_count = 0;
    for(p = proc; p < &proc[NPROC]; p++)
    {
      if (p->state == RUNNABLE)
        ticket_count += p->tickets;

      if (ticket_count > winner) break;
    }

    if (p)
    {
      acquire(&p->lock);

      if(p->state == RUNNABLE) {
      // Switch to chosen process.  It is the process's job
      // to release its lock and then reacquire it
      // before jumping back to us.
      p->state = RUNNING;
      c->proc = p;
      swtch(&c->context, &p->context);

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
      }
      release(&p->lock);
    }

  }

  #elif PBS

  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;)
  {
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();


    struct proc* nextP = 0;
    int dynamic_priority;
    int niceness, check_dp = 420;

    for (p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);

      if (p->run_time || p->sleep_time)
        niceness = (p->sleep_time*10 / (p->sleep_time + p->run_time));
      else 
        niceness = 5;

      dynamic_priority = max(0, min(p->static_priority - niceness + 5, 100));

      if (p->state == RUNNABLE)
      {
        if (!nextP || dynamic_priority > check_dp ||
            (dynamic_priority == check_dp && p->times_scheduled > nextP->times_scheduled) ||
            (dynamic_priority == check_dp && p->times_scheduled > nextP->times_scheduled && p->creation_time > nextP->creation_time))
        {
          check_dp = dynamic_priority;

          if (nextP) release(&nextP->lock);
          nextP = p;

          continue; // don't release nextP's lock
        }
      }

      release(&p->lock);
    }

    if (nextP)
    {
      // still holding nextP's lock, no need to acquire

      p = nextP;
      p->times_scheduled++;
      p->run_time = 0;
      p->sleep_time = 0;

      // Switch to chosen process.  It is the process's job
      // to release its lock and then reacquire it
      // before jumping back to us.
      p->state = RUNNING;
      c->proc = p;
      swtch(&c->context, &p->context);

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;

      release(&p->lock);
    }

  }

  #elif MLFQ
struct cpu *c = mycpu();
  if(debug)printf("mlfq\n");

  c->proc = 0;
  intr_on();

  for(;;)
  {
    CheckRunnable();
    CheckAging();
    // printf("e0 = %d , e1 = %d\n",e0,e1);
    if (e0 != 0)
    {
      struct proc *p = q0[0];
        if(debug)printf("process p:%p %s in q0\n",p,p->name);
      acquire(&p->lock);

      if (p->state == RUNNABLE)
      {
        if(debug)printf("running\n");
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      if(debug)printf("run over\n");
      p->mticks++;
      if (p->state != RUNNABLE)
      {
        if(debug)printf("unrunnable \n");
        qpop(q0, 0, e0);
        e0--;
      }
      else if (p->mticks == time_Slice[0])
      {
        qpop(q0, 0, e0);
        qpush_back(q1, p, e1);
        e1++;
        e0--;
        p->mticks = 0;
        p->queue_no = 1;
        struct proc* curproc = p;
        if (DATA)
        {
          printf("%d %d %d\n", ticks, curproc->pid, curproc->queue_no);
        }
        if(debug)printf("process p:%p pused to q1 and e0 = %d and e1 = %d \n",p,e0,e1);
      }
      release(&p->lock);

    }
    if (e1 != 0)
    {
      struct proc *p = q1[0];
        if(debug)printf("process p:%p %s in q1\n",p,p->name);
      acquire(&p->lock);

      if (p->state == RUNNABLE)
      {
        if(debug)printf("running\n");
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      if(debug)printf("run over\n");
      p->mticks++;
      if (p->state != RUNNABLE)
      {
        if(debug)printf("unrunnable \n");
        qpop(q1, 0, e1);
        e1--;
      }
      else if (p->mticks == time_Slice[1])
      {
        qpop(q1, 0, e1);
        qpush_back(q2, p, e2);
        e2++;
        e1--;
        p->mticks = 0;
        p->queue_no = 2;
          struct proc* curproc = p;
          if (DATA)
          {
            printf("%d %d %d\n", ticks, curproc->pid, curproc->queue_no);
          }
        if(debug)printf("process p:%p pused to q2 and e1 = %d and e2 = %d \n",p,e1,e2);
      }
      release(&p->lock);

    }
    if (e2 != 0)
    {
      struct proc *p = q2[0];
        if(debug)printf("process p:%p %s in q2\n",p,p->name);
      acquire(&p->lock);

      if (p->state == RUNNABLE)
      {
        if(debug)printf("running\n");
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      if(debug)printf("run over\n");
      p->mticks++;
      if (p->state != RUNNABLE)
      {
        if(debug)printf("unrunnable \n");
        qpop(q2, 0, e2);
        e2--;
      }
      else if (p->mticks == time_Slice[2])
      {
        qpop(q2, 0, e2);
        qpush_back(q3, p, e3);
        e3++;
        e1--;
        p->mticks = 0;
        p->queue_no = 3;
          struct proc* curproc = p;
          if (DATA)
          {
            printf("%d %d %d\n", ticks, curproc->pid, curproc->queue_no);
          }
        if(debug)printf("process p:%p pused to q3 and e2 = %d and e3= %d \n",p,e2,e3);
      }
      release(&p->lock);

    }
    if (e3 != 0)
    {
      struct proc *p = q3[0];
        if(debug)printf("process p:%p %s in q3\n",p,p->name);
      acquire(&p->lock);

      if (p->state == RUNNABLE)
      {
        if(debug)printf("running\n");
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      if(debug)printf("run over\n");
      p->mticks++;
      if (p->state != RUNNABLE)
      {
        if(debug)printf("unrunnable \n");
        qpop(q3, 0, e3);
        e3--;
      }
      else if (p->mticks == time_Slice[3])
      {
        qpop(q3, 0, e3);
        qpush_back(q4, p, e4);
        e4++;
        e3--;
        p->mticks = 0;
        p->queue_no = 4;
          struct proc* curproc = p;
          if (DATA)
          {
            printf("%d %d %d\n", ticks, curproc->pid, curproc->queue_no);
          }
        if(debug)printf("process p:%p pused to q4 and e3 = %d and e4 = %d \n",p,e3,e4);
      }
      release(&p->lock);

    }
    else if (e4 != 0)
    {
      struct proc *p = q4[0];
        if(debug)printf("process p:%p %s in q4\n",p,p->name);
      acquire(&p->lock);

      if (p->state == RUNNABLE)
      {
        if(debug)printf("running\n");
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }

      if(debug)printf("run over\n");
      // p->mticks++;
      if (p->state != 3)
      {
        if(debug)printf("unrunnable as %s's pstate is %d\n",p->name,p->state);
        qpop(q4,0,e4);
        e4--;
      }
      else if (p->mticks == time_Slice[4])
      {
        qpop_and_push_back(q4,0,e4);
      }

      release(&p->lock);

    }
    update_minfo();
  }
  #else
  while(1){}
      #endif

}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++) {
    if(p != myproc()){
      acquire(&p->lock);
      if(p->state == SLEEPING && p->chan == chan) {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kill(int pid)
{
  struct proc *p;

  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING){
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if(user_dst){
    return copyout(p->pagetable, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if(user_src){
    return copyin(p->pagetable, dst, src, len);
  } else {
    memmove(dst, (char*)src, len);
    return 0;
  }
}

void QueueDetails()
{
  printf("Queue Details - \n");

  printf("Queue 0 : ");
  for(int i = 0 ; i < e0; i++)
  {
    printf("%s ",q0[i]->name);
  }
  printf("\n");

  printf("Queue 1 : ");
  for(int i = 0 ; i < e0; i++)
  {
    printf("%s ",q1[i]->name);
  }
  printf("\n");

  printf("Queue 2 : ");
  for(int i = 0 ; i < e0; i++)
  {
    printf("%s ",q2[i]->name);
  }
  printf("\n");

  printf("Queue 3 : ");
  for(int i = 0 ; i < e0; i++)
  {
    printf("%s ",q3[i]->name);
  }
  printf("\n");

  printf("Queue 4 : ");
  for(int i = 0 ; i < e0; i++)
  {
    printf("%s ",q4[i]->name);
  }
  printf("\n");

}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for(p = proc; p < &proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s(%d) %s", p->pid, state, p->state, p->name);
    printf("\n");
  }

  #ifdef MLFQ
  // QueueDetails();
  // print_minfo();
  #endif
}


// Changed CheckRunnable with q_no and still works - here check Aging was commented out
// In CheckAging release before continuing


// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
waitx(uint64 addr, uint* wtime, uint* rtime)
{
  struct proc *np;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(np = proc; np < &proc[NPROC]; np++){
      if(np->parent == p){
        // make sure the child isn't still in exit() or swtch().
        acquire(&np->lock);

        havekids = 1;
        if(np->state == ZOMBIE){
          // Found one.
          pid = np->pid;
          *rtime = np->rtime;
          *wtime = np->etime - np->ctime - np->rtime;
          if(addr != 0 && copyout(p->pagetable, addr, (char *)&np->xstate,
                                  sizeof(np->xstate)) < 0) {
            release(&np->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(np);
          release(&np->lock);
          release(&wait_lock);
          return pid;
        }
        release(&np->lock);
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || p->killed){
      release(&wait_lock);
      return -1;
    }

    // Wait for a child to exit.
    sleep(p, &wait_lock);  //DOC: wait-sleep
  }
}

void
update_time()
{
  struct proc* p;
  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state == RUNNING) {
      p->rtime++;
    }
    release(&p->lock); 
  }
}

int settickets(int number)
{
  if (number < 0)
    return -1;

  int tmp = myproc()->tickets;
  myproc()->tickets += number;

  return tmp + number;
}

int setpriority(int new_priority, int pid)
{
  if (new_priority < 0 || pid < 0)
    return -1;

  int old_priority = -1;
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);

    if (p->pid == pid && new_priority >= 0 && new_priority <= 100)
    {
      old_priority = p->static_priority;
      p->run_time = 0;
      p->sleep_time = 0;

      p->static_priority = new_priority;

      release(&p->lock);
      // if (new_priority < old_priority) yield();

      return old_priority;
    }

    release(&p->lock);
  }

  // process with given pid not found
  return -1;
}

void update_proc_info()
{
  struct proc *p;
  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);

    if (p->state == RUNNING)
    {
      p->run_time++;
    }
    if (p->state == SLEEPING)
    {
      p->sleep_time++;
    }

    release(&p->lock);
  }
}

// int uwu[NPROC][10000] = {0};

void update_minfo()
{
  // struct proc* p;
  // for( p = proc; p < &proc[NPROC]; p++)
  // {
  //   acquire(&p->lock);
  //   if(CheckPresent(p) == 1)
  //   {
  //     uwu[p->pid][ticks] = p->queue_no;
  //     // if(p->queue_no != 0) printf("sdgdsfg\n");
  //     uwu[p->pid][0]++; 
  //   }
  //   release(&p->lock);
  // }
}

void print_minfo()
{
  // struct proc* p;
  // for( p = proc; p < &proc[NPROC]; p++)
  // {
  //   acquire(&p->lock);
  //   if(p->state != SLEEPING) 
  //   {
  //     release(&p->lock);
  //     continue;
  //   }
  //   printf("\n pid = %p and p =%d : \n",p, p->pid);
  //   int size = uwu[p->pid][0];
  //   for(int i = 1; i < size; i++)
  //   {
  //     printf("at tick %d , queue no = %d \n",i,uwu[p->pid][i]);
  //   }
  //   release(&p->lock);
  // }
}