// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKETS 13

struct
{
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf hashbucket[NBUCKETS]; //每个哈希队列一个linked list及一个lock
} bcache;

void binsert(struct buf *b, int nbucket)
{
  b->next = bcache.hashbucket[nbucket].next;
  b->prev = &bcache.hashbucket[nbucket];
  initsleeplock(&b->lock, "buffer");
  bcache.hashbucket[nbucket].next->prev = b;
  bcache.hashbucket[nbucket].next = b;
}

void bmov(struct buf *b, int nbucket)
{
  b->next->prev = b->prev;
  b->prev->next = b->next;
  b->next = bcache.hashbucket[nbucket].next;
  b->prev = &bcache.hashbucket[nbucket];
  bcache.hashbucket[nbucket].next->prev = b;
  bcache.hashbucket[nbucket].next = b;
}

void binit(void)
{
  struct buf *b = bcache.buf;
  int nbuf = NBUF / NBUCKETS;

  for (int i = 0; i < NBUCKETS; i++)
  {
    initlock(&bcache.lock[i], "bcache_");

    // Create linked list of buffers
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];

    if (i == NBUCKETS - 1)
    {
      for (; b < bcache.buf + NBUF; b++)
        binsert(b, i);
    }
    else
    {
      for (int j = 0; j < nbuf; j++, b++)
        binsert(b, i);
    }
  }
}

int hashmap(uint dev, uint blockno)
{
  return (dev * 11 + blockno) % NBUCKETS;
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *b;
  int nbucket = hashmap(dev, blockno);

  acquire(&bcache.lock[nbucket]);

  // Is the block already cached?
  for (b = bcache.hashbucket[nbucket].next; b != &bcache.hashbucket[nbucket]; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      b->refcnt++;
      release(&bcache.lock[nbucket]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (b = bcache.hashbucket[nbucket].prev; b != &bcache.hashbucket[nbucket]; b = b->prev)
  {
    if (b->refcnt == 0)
    {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[nbucket]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Steal from other hashbucket
  release(&bcache.lock[nbucket]);
  for (int nb = (nbucket + 1) % NBUCKETS; nb != nbucket; nb = (nb + 1) % NBUCKETS)
  {
    acquire(&bcache.lock[nb]);
    for (b = bcache.hashbucket[nb].prev; b != &bcache.hashbucket[nb]; b = b->prev)
    {
      if (b->refcnt == 0)
      {
        if (nb < nbucket)
        {
          acquire(&bcache.lock[nbucket]);
        }
        else
        {
          release(&bcache.lock[nb]);
          acquire(&bcache.lock[nbucket]);
          acquire(&bcache.lock[nb]);
        }
        bmov(b, nbucket);
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        release(&bcache.lock[nb]);
        release(&bcache.lock[nbucket]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.lock[nb]);
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int nbucket = hashmap(b->dev, b->blockno);

  acquire(&bcache.lock[nbucket]);
  b->refcnt--;
  if (b->refcnt == 0)
  {
    // no one is waiting for it.
    bmov(b, nbucket);
  }

  release(&bcache.lock[nbucket]);
}

void bpin(struct buf *b)
{
  int nbucket = hashmap(b->dev, b->blockno);
  acquire(&bcache.lock[nbucket]);
  b->refcnt++;
  release(&bcache.lock[nbucket]);
}

void bunpin(struct buf *b)
{
  int nbucket = hashmap(b->dev, b->blockno);
  acquire(&bcache.lock[nbucket]);
  b->refcnt--;
  release(&bcache.lock[nbucket]);
}
