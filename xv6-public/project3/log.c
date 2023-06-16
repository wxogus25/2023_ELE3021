#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit

struct logheader {
  int n;
  int block[LOGSIZE];
};

struct log {
  struct spinlock lock;
  int start;
  int size;
  int outstanding; // how many FS sys calls are executing.
  int committing;  // in commit(), please wait.
  int dev;
  struct logheader lh;
};
struct log log;

extern struct {
  struct spinlock lock;
  struct buf buf[RNBUF];

  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
  struct buf head;
} bcache;

static void recover_from_log(void);
static int dirty_num;

static void commit();

void
initlog(int dev)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  struct superblock sb;
  initlock(&log.lock, "log");
  readsb(dev, &sb);
  log.start = sb.logstart;
  log.size = sb.nlog;
  log.dev = dev;
  recover_from_log();
}

// Copy committed blocks from log to their home location
static void
install_trans(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *lbuf = bread(log.dev, log.start+tail+1); // read log block
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]);  // read dst
    memmove(dbuf->data, lbuf->data, BSIZE);                 // copy block to dst
    bwrite(dbuf);                                           // write dst to disk
    brelse(lbuf);
    brelse(dbuf);
  }
}

// Read the log header from disk into the in-memory log header
static void
read_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
static void
write_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *hb = (struct logheader *) (buf->data);
  int i;
  hb->n = log.lh.n;
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }
  bwrite(buf);
  brelse(buf);
}

static void
recover_from_log(void)
{
  read_head();
  install_trans(); // if committed, copy from log to disk
  log.lh.n = 0;
  write_head(); // clear the log
}

// buf 제한 체크하고 넘으면 flush
void
limit_check()
{
  // 버퍼 크기 넘어서면 dirty bit 켜진 버퍼들 commit
  if(log.lh.n >= NBUF){
    acquire(&log.lock);
    log.committing = 1;
    release(&log.lock);
    commit();
    acquire(&log.lock);
    log.committing = 0;
    wakeup(&log);
    release(&log.lock);
    dirty_num = 0;
  }
}

// called at the start of each FS system call.
// 모든 파일 처리는 begin과 end 사이에서 이루어짐
// 트랜잭션하고 비슷함
void
begin_op(void)
{
  acquire(&log.lock);
  while(1){
    // commit 중인지 확인(flush 하는지)
    if(log.committing){
      sleep(&log, &log.lock);
      // log에 데이터 쓸 수 있는 상태인지 확인
    } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE){
      // this op might exhaust log space; wait for commit.
      sleep(&log, &log.lock);
    } else {
      // 아무도 안쓰고 있고, log에도 공간이 있으면, 파일시스템에 쓰기 시작
      if(log.outstanding == 0){
        release(&log.lock);
        // BUF 제한 넘겼나 확인
        limit_check();
        acquire(&log.lock);
      }
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
void
end_op(void)
{
  acquire(&log.lock);
  // 값 다 썼으니까 log에서 값 제거
  log.outstanding -= 1;
  // 내가 쓰고있는데 다른 놈이 쓰는건 말이 안 됨
  if(log.committing)
    panic("log.committing");
    
  if(log.outstanding != 0){
    wakeup(&log);
  }
  release(&log.lock);
}
// log : 데이터 디스크에 쓰기 전에 로그에 씀(버퍼 -> 로그 -> 디스크)
// sync가 호출 될 때만 버퍼가 디스크에 내려가게 바꾸면 됨

// Copy modified blocks from cache to log.
static void
write_log(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *to = bread(log.dev, log.start+tail+1); // log block
    struct buf *from = bread(log.dev, log.lh.block[tail]); // cache block
    memmove(to->data, from->data, BSIZE);
    bwrite(to);  // write the log
    if(from->flags & B_DIRTY){
      dirty_num++;
    }
    brelse(from);
    brelse(to);
  }
}

void
static commit()
{
  if (log.lh.n > 0) {
    write_log();     // Write modified blocks from cache to log
    write_head();    // Write header to disk -- the real commit
    install_trans(); // Now install writes to home locations
    log.lh.n = 0;
    write_head();    // Erase the transaction from the log
  }
}

// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache with B_DIRTY.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   brelse(bp)
void
log_write(struct buf *b)
{
  int i;

  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
    panic("too big a transaction");
  if (log.outstanding < 1)
    panic("log_write outside of trans");

  acquire(&log.lock);
  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno)   // log absorbtion
      break;
  }
  log.lh.block[i] = b->blockno;
  if (i == log.lh.n)
    log.lh.n++;
  b->flags |= B_DIRTY; // prevent eviction
  release(&log.lock);
}

int
sys_sync(void)
{
  int temp = -1;
  acquire(&log.lock);
  log.committing = 1;
  release(&log.lock);
  commit();
  acquire(&log.lock);
  log.committing = 0;
  wakeup(&log);
  release(&log.lock);
  temp = dirty_num;
  dirty_num = 0;
  return temp; 
}