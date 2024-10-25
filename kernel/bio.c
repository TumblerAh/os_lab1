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

struct {
  struct spinlock wholelock; //全局分配大锁
  struct spinlock lock[NBUCKETS]; //自旋锁，用于用户互斥访问
  struct buf buf[NBUF];  //双向链表,内存缓存块
  struct buf hashbucket[NBUCKETS]; //每个哈希队列一个linked list 以及一个lock
} bcache;

uint 
hash(uint blockno){
  return blockno % NBUCKETS;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.wholelock, "bcache");

  for(int i=0;i<NBUCKETS;i++){
    initlock(&bcache.lock[i], "bcache"+i);
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }

  for(b = bcache.buf;b<bcache.buf+NBUF;b++){
    uint index = hash(b->blockno);
    b->next = bcache.hashbucket[index].next;
    b->prev = &bcache.hashbucket[index];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[index].next->prev = b;
    bcache.hashbucket[index].next=b;
  }

  
  // // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = &head[b->blockno%NBUCKETS].next;
  //   b->prev = &head[b->blockno%NBUCKETS];
  //   initsleeplock(&b->lock, "buffer");
  //   head[b->blockno%NBUCKETS].next->prev = b;
  //   head[b->blockno%NBUCKETS].next = b;
  // }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
//检查请求的磁盘块是不是在缓存中
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint index = hash(blockno);
  acquire(&bcache.lock[index]);

  // Is the block already cached?
  for(b = bcache.hashbucket[index].next; b != &bcache.hashbucket[index]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[index]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  
  //本桶中有可以替换的数据块
  for(b = bcache.hashbucket[index].prev;b!=&bcache.hashbucket[index];b=b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[index]);
      acquiresleep(&b->lock);
      return b;   
    } 
  }

  release(&bcache.lock[index]);

  acquire(&bcache.wholelock); //获取全局分配大锁
  
  //如果本桶没有，则从其他桶寻找
  for(int i=0;i<NBUCKETS;i++){
    if(i == index) continue;
    else{
      acquire(&bcache.lock[i]); 

      for(b = bcache.hashbucket[i].prev;b!=&bcache.hashbucket[i];b=b->prev){
        if(b->refcnt == 0){
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1;      

          //从当前桶中删除
          b->next->prev = b->prev;
          b->prev->next = b->next;
          
          acquire(&bcache.lock[index]);//获取原有桶的锁
          
          //加入目标桶
          b->next = bcache.hashbucket[index].next;
          b->prev = &bcache.hashbucket[index];
          bcache.hashbucket[index].next->prev = b;
          bcache.hashbucket[index].next=b;
          
          release(&bcache.lock[index]);
          release(&bcache.lock[i]);
          release(&bcache.wholelock);
          acquiresleep(&b->lock);
          return b;

        }
      }
      release(&bcache.lock[i]);
      
    }

  }
  release(&bcache.wholelock);
  // for(b = head[i].prev; b != &head[i]; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock[i]);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
 
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  uint index = hash(b->blockno);
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);  

  acquire(&bcache.lock[index]);
  b->refcnt--;
  //一个block cache最近被使用过，很可能会再次被使用
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbucket[index].next;
    b->prev = &bcache.hashbucket[index];
    bcache.hashbucket[index].next->prev = b;
    bcache.hashbucket[index].next = b;
  }
  
  release(&bcache.lock[index]);
}

void
bpin(struct buf *b) {
  uint index = hash(b->blockno);
  acquire(&bcache.lock[index]);
  b->refcnt++;
  release(&bcache.lock[index]);
}

void
bunpin(struct buf *b) {
  uint index = hash(b->blockno);
  acquire(&bcache.lock[index]);
  b->refcnt--;
  release(&bcache.lock[index]);
}


