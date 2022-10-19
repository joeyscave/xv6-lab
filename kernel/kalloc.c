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

struct run
{
  struct run *next;
};

struct kmem
{
  struct spinlock lock;
  struct run *freelist;
};

struct kmem kmems[NCPU];

void kinit()
{
  for (int i = 0; i < NCPU; i++)
  {
    initlock(&kmems[i].lock, "kmem_"); // TODO
  }
  freerange(end, (void *)PHYSTOP);
}

// kfree exclusively used for specific cpu
void cpukfree(void *pa, int cpuid)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(&kmems[cpuid].lock);
  r->next = kmems[cpuid].freelist;
  kmems[cpuid].freelist = r;
  release(&kmems[cpuid].lock);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  cpukfree(pa, cpuid());
}

// kalloc exclusively used for specific cpu
void *
cpukalloc(int cpuid)
{
  struct run *r;

  acquire(&kmems[cpuid].lock);
  r = kmems[cpuid].freelist;
  if (r)
  {
    kmems[cpuid].freelist = r->next;
    release(&kmems[cpuid].lock);
  }
  else
  {
    // steal memory from other CPU
    release(&kmems[cpuid].lock);
    for (int id = (cpuid + 1) % NCPU; id != cpuid; id = (id + 1) % NCPU)
    {
      acquire(&kmems[id].lock);
      r = kmems[id].freelist;
      if (r)
      {
        kmems[id].freelist = r->next;
        release(&kmems[id].lock);
        break;
      }
      release(&kmems[id].lock);
    }
  }

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  return cpukalloc(cpuid());
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  int j, npage = (int)((char *)pa_end - p) / PGSIZE / NCPU;

  for (int i = 0; i < NCPU - 1; i++)
  {
    for (j = 0; j < npage; p += PGSIZE, j++)
      cpukfree(p, i);
  }
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    cpukfree(p, NCPU - 1);
}