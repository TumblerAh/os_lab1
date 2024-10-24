// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem{
  struct spinlock lock;
  struct run *freelist;
};
struct kmem kmems[NCPU];  //每个cpu拥有独立的内存链表



void
kinit()
{
  push_off();
  int cpu_id = cpuid();
  pop_off();
  initlock(&kmems[cpu_id].lock, "kmem"+cpu_id);
  //初始化时先把所有的空闲内存页放到第一个cpu链表中
    freerange(end, (void*)PHYSTOP);
}

//为所有运行freerange的cpu分配空闲的内存
void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
//释放内存空间
void
kfree(void *pa)
{
  push_off();
  int cpu_id = cpuid();
  pop_off();
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmems[cpu_id].lock);
  r->next = kmems[cpu_id].freelist;
  kmems[cpu_id].freelist = r;
  release(&kmems[cpu_id].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
//申请内存，分配内存,优先分配当前cpu的freelist,否则进行窃取
void *
kalloc(void)
{
  push_off();
  int cpu_id = cpuid();
  pop_off();

  struct run *r;

  acquire(&kmems[cpu_id].lock);
  r = kmems[cpu_id].freelist;
  if(r){
      kmems[cpu_id].freelist = r->next;
      release(&kmems[cpu_id].lock);
  }
  else{
      release(&kmems[cpu_id].lock);
      for(int i=0;i<NCPU;i++){
          if(i == cpu_id){
            continue;
          }else{
            acquire(&kmems[i].lock);
              r = kmems[i].freelist;
              if(r){
                kmems[i].freelist = r->next;
                release(&kmems[i].lock);
                break;
              }
            release(&kmems[i].lock);
          }
      }
      
  }
  // release(&kmems[cpuid].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
