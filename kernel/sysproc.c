#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
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


// #ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 buf_addr;
  int npage;
  uint64 bitmask_addr_uvm;
  uint bitmask = 0;
  argaddr(0, &buf_addr);
  argint(1, &npage);
  argaddr(2, &bitmask_addr_uvm);

  for (int i = 0; i < npage; i++) {
    pte_t *pte_p = walk(myproc()->pagetable, 
                        buf_addr + i * PGSIZE, 0);
    
    if ((pte_p != 0) && ((*pte_p) & PTE_A)) {
      bitmask = bitmask | (1 << i);
      (*pte_p) = (*pte_p) & ~PTE_A;
    }
  }
  printf("kernel res: %x\n", bitmask);
  if(copyout(myproc()->pagetable, bitmask_addr_uvm, (char *)&bitmask, sizeof(uint)))
    panic("sys_pgacess copyout error");

  return 0;


  // lab pgtbl: your code here.
  int n;  // number of page
  // bufAddr: the first user page
  // abitsAddr
  uint64 bufAddr, abits, bitsmask=0;
  // 依次将系统调用的参数取出并保存在相应的变量中
    argaddr(2, &abits);  
  argint(1, &n);
  argaddr(0, &bufAddr);
  struct proc *p = myproc();

  for(int i=0;i<n;i++){
    pte_t *pte = walk(p->pagetable, bufAddr, 0);
    if(*pte & PTE_A) { // accessed
      bitsmask |= (1L << i);
    }
    // clear the PTE_A
    *pte &= ~PTE_A;
    bufAddr += PGSIZE;
  }
  if(copyout(p->pagetable, abits, (char *)&bitsmask, sizeof(bitsmask)) < 0)
    panic("sys_pgacess copyout error");
  return 0;
}
// #endif

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
