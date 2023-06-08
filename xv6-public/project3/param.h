#define NPROC        64  // maximum number of processes
#define KSTACKSIZE 4096  // size of per-process kernel stack
#define NCPU          8  // maximum number of CPUs
#define NOFILE       16  // open files per process
#define NFILE       100  // open files per system
#define NINODE       50  // maximum number of active i-nodes
#define NDEV         10  // maximum major device number
#define ROOTDEV       1  // device number of file system root disk
#define MAXARG       32  // max exec arguments
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS*3)  // max data blocks in on-disk log
// 버퍼 키우고싶을때 만지면 됨
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache
// FSSIZE 무조건 바꿔야 함
#define FSSIZE       1000  // size of file system in blocks
#define MAXPAGE      2048  // 최대 페이지 개수
#define MAXTHREAD    64
