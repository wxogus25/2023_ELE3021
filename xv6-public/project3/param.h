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
#define NBUF         (MAXOPBLOCKS*3)   // size of disk block cache
#define RNBUF        (MAXOPBLOCKS*3) + 20  // NBUF에 여유 블럭 추가함
// LOGSIZE에 여유 블럭 추가함
#define LOGSIZE      RNBUF       // max data blocks in on-disk log
// MAXFILE과 동일하게 설정해둠
#define FSSIZE 2113548  // size of file system in blocks