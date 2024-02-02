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
// start--------------------------
#define NBUCK 13
struct {
  struct spinlock lock;
  struct buf head;
} bcache_bucks[NBUCK];


// end  ---------------------------

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  // -----------------------------
  for (int i = 0; i < NBUCK; i++) {
    initlock(&bcache_bucks[i].lock, "bcache.buck");
    // linklist init
    bcache_bucks[i].head.next = &bcache_bucks[i].head;
    bcache_bucks[i].head.prev = &bcache_bucks[i].head;
  }
  b = bcache.buf;
  for (int i = 0; i < NBUF; i++) {
    int hash_id = i % NBUCK;
    b->next = bcache_bucks[hash_id].head.next;
    b->prev = &(bcache_bucks[hash_id].head);
    initsleeplock(&b->lock, "buffer");
    bcache_bucks[hash_id].head.next->prev = b;
    bcache_bucks[hash_id].head.next = b;

    b++;
  }


  // // ---------------------------
  // // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  // acquire(&bcache.lock);
  
  // ----------------------
  int hash_id = blockno % NBUCK;
  acquire(&(bcache_bucks[hash_id].lock));
  for (b = bcache_bucks[hash_id].head.next; b != &(bcache_bucks[hash_id].head); b = b->next) {
    // printf("1pass bno:%d\n", b->blockno);
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      // printf("find already refcnt:%d, bno:%d \n", b->refcnt, b->blockno);
      release(&(bcache_bucks[hash_id].lock));
      // release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  for (b = bcache_bucks[hash_id].head.next; b != &(bcache_bucks[hash_id].head); b = b->next) {
    // printf("2pass bno:%d\n", b->blockno);
    if (b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        // printf("find new bno: %d \n", b->blockno);
        release(&(bcache_bucks[hash_id].lock));
        acquiresleep(&b->lock);
        return b;
      }
  }
  release(&(bcache_bucks[hash_id].lock));

  int ret_hash_id = -1;
  for (int k = 0; k < NBUCK - 1; k++) {
    int i = (hash_id + k + 1) % NBUCK;
    acquire(&(bcache_bucks[i].lock));

    for (b = bcache_bucks[i].head.next; b!= &(bcache_bucks[i].head); b = b->next) {
      if (b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        // printf("find new bno: %d, in new hash bucket\n", b->blockno);
        b->next->prev = b->prev;
        b->prev->next = b->next;
        
        ret_hash_id = i;
        break;
      }
    }
    release(&(bcache_bucks[i].lock));
    if (ret_hash_id != -1) {
      break;
    }
  }

  if (!(b->dev == dev && b->blockno == blockno))
    panic("bget: no buffers");

  
  ret_hash_id = -ret_hash_id;
  // printf("ret hash: %d\n", ret_hash_id);
  acquire(&(bcache_bucks[hash_id].lock));
  struct buf *b1;
  for (b1 = bcache_bucks[hash_id].head.next; b1 != &(bcache_bucks[hash_id].head); b1 = b1->next) {
    if (b1->dev == dev && b1->blockno == blockno) {
        b1->refcnt++;
        ret_hash_id = -ret_hash_id;

        release(&(bcache_bucks[hash_id].lock));
        break;
      }
  }
  if (ret_hash_id > 0) {
    b->refcnt = 0;
    acquire(&(bcache_bucks[ret_hash_id].lock));
    b->next = bcache_bucks[ret_hash_id].head.next;
    b->prev = &(bcache_bucks[ret_hash_id].head);
    bcache_bucks[ret_hash_id].head.next->prev = b;
    bcache_bucks[ret_hash_id].head.next = b;
    release(&(bcache_bucks[ret_hash_id].lock));
    b = b1;
  } else {
    b->next = bcache_bucks[hash_id].head.next;
    b->prev = &(bcache_bucks[hash_id].head);
    bcache_bucks[hash_id].head.next->prev = b;
    bcache_bucks[hash_id].head.next = b;
    release(&(bcache_bucks[hash_id].lock));
  }
  
  // release(&bcache.lock);
  acquiresleep(&(b->lock));
  return b;
  // ----------------------
  
  // // Is the block already cached?
  // for(b = bcache.head.next; b != &bcache.head; b = b->next){
  //   if(b->dev == dev && b->blockno == blockno){
  //     b->refcnt++;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }

  // // Not cached.
  // // Recycle the least recently used (LRU) unused buffer.
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  // panic("bget: no buffers");
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
  
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  // printf("release bno: %d\n", b->blockno);
  // acquire(&bcache.lock);
  int hash_id = b->blockno % NBUCK;
  acquire(&(bcache_bucks[hash_id].lock));
  b->refcnt--;
  // if (b->refcnt == 0) {
  //   // no one is waiting for it.
  //   b->next->prev = b->prev;
  //   b->prev->next = b->next;
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
  // release(&bcache.lock);
  release(&(bcache_bucks[hash_id].lock));
}

void
bpin(struct buf *b) {
  int hash_id = b->blockno % NBUCK;
  acquire(&(bcache_bucks[hash_id].lock));
  b->refcnt++;
  release(&(bcache_bucks[hash_id].lock));
}

void
bunpin(struct buf *b) {
    int hash_id = b->blockno % NBUCK;
 acquire(&(bcache_bucks[hash_id].lock));
  b->refcnt--;
  release(&(bcache_bucks[hash_id].lock));
}


