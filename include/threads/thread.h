#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#define USERPROG
#define VM
//#define DEBUG

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"

#ifdef USERPROG
#include "threads/synch.h"
#include "filesys/file.h"

// struct fdt_info {
// 	struct file **fdt;
// 	int size;
// };

#endif

#ifdef VM
#include "vm/vm.h"
#endif



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

	/* Shared between thread.c and synch.c. */
	struct list_elem elem;              /* List element. */

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */
	
	// implement for system call
	// struct thread *parent;
	struct semaphore fork_sema;
	struct semaphore wait_sema;
	struct semaphore exit_sema;
	struct list_elem child_elem;
	struct list child_list;
	int exit_status;
	struct file **fdt;
	int fdt_cur;
	int num_files;
	bool is_process;
	struct intr_frame parent_if;
	struct file *running_file;
	struct lock* filesys_lock;
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
	struct hash mmap_table;
	void *user_rsp;
	struct lock *swap_lock;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf;               /* Information for switching */
	unsigned magic;                     /* Detects stack overflow. */

	// implement for alarm clock
	int64_t wake_up_tick;               /* Wake up tick for alarm clock */

	// implement for priority donation
	int original_priority;              /* Original priority */
	struct lock *waiting_lock;          /* Lock that the thread is waiting for */
	struct list donations;              /* List of donations */
	struct list_elem d_elem;     /* List element for donation list */

	// implement for mlfqs scheduling
	int recent_cpu_time;
	int nice;
	struct list_elem all_elem;
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

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);

// implement for multilevel queue
bool list_empty_readyList(void);
size_t list_size_readyList(void);
struct list_elem *list_pop_front_readyList(void);
struct list_elem *list_begin_readyList(void);

//implement
bool is_priority_of_curThread_highest (void);

// implement for alarm clock
bool compare_wake_up_tick(const struct list_elem *, const struct list_elem *, void *aux);
void thread_sleep (struct thread *, int64_t ticks);
void thread_wake_up (int64_t ticks);

// implement for priority scheduling
bool compare_priority(const struct list_elem *, const struct list_elem *, void *aux);

// implement for priority donations
bool compare_priority_in_donations(const struct list_elem *, const struct list_elem *, void *aux);
void donate_priority(struct thread *);
void reset_priority(void);
void reset_donations(struct lock *);

// implement for mlfqs schedulling
void increase_recent_cpu_time(void);
void update_load_avg(void);
int calculate_priority(struct thread *t);
void update_priority(void);
void update_recent_cpu_time(void);

// implement for user process
struct thread *get_thread(tid_t tid);

#endif /* threads/thread.h */
