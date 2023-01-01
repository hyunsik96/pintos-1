#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#ifdef VM
#include "vm/vm.h"
#endif

/* project2 */
#define FDT_PAGES 3
#define FDT_COUNT_LIMIT FDT_PAGES *(1<<9) // limit fdidx

/* States in a thread's life cycle. */
enum thread_status {
	THREAD_RUNNING,     /* Running thread. */
	THREAD_READY,       /* Not running but ready to run. */
	THREAD_BLOCKED,     /* Waiting for an event to trigger. */
	THREAD_DYING        /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread {
	/* Owned by thread.c. */
	tid_t tid;                          /* Thread identifier. */
	enum thread_status status;          /* Thread state. */
	char name[16];                      /* Name (for debugging purposes). */
	int priority;                       /* Priority. */
	int pre_priority;					/* donate 받기 이전, 기존 우선순위 */
	int64_t wakeup_tick;				/* 추가 */
	struct list_elem elem;              /* List element. */
	
	// 해당 쓰레드가 대기하고 있는 lock 자료구조 주소 저장필드
	struct lock* wait_on_lock;
	struct list donations;
	struct list_elem d_elem;
	
	/* project2 system call */
	int exit_status;	// exit 할때 status 넣어주는 필드
	struct file **fd_table;	// file descriptor table 의 시작 주소를 가르킴
	int fd_idx;	//	fd table 의 open spot 의 index

	struct intr_frame parent_if;	// 부모 쓰레드의 if
	struct semaphore fork_sema;	// fork한 child의 load를 기다리는 용도

	struct list child_list;	// parent가 가진 자식 쓰레드 리스트
	struct list_elem child_elem;

	struct semaphore wait_sema;
	struct semaphore free_sema;

	struct file *running;	// 이 스레드에서 실행시키고있는 파일

	// int stdin_count;
	// int stdout_count;

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf;               /* Information for switching */
	unsigned magic;                     /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);
void thread_sleep(int64_t ticks);	/* 재우는 함수 추가 */

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

/* 비교 함수 */
bool cmp_priority(const struct list_elem *a,
const struct list_elem *b,void *aux UNUSED);
bool d_cmp_priority(const struct list_elem *a,
const struct list_elem *b,void *aux UNUSED);

/* 실행 중인 스레드를 레디큐 가장 앞녀석 우선순위 비교해서 더 작으면 yield 시키기 */
void test_max_priority(void);

void do_iret (struct intr_frame *tf);
/* thread.c의 next_tick_to_awake반환*/
int64_t get_next_tick_to_awake(void);
 /*최소틱을가진 스레드저장*/
void update_next_tick_to_awake(int64_t ticks);
/* 슬립큐에서깨워야할스레드를깨움*/
void thread_awake(int64_t ticks); 
#endif /* threads/thread.h */