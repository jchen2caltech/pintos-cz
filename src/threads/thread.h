/*! \file thread.h
 *
 * Declarations for the kernel threading functionality in PintOS.
 */

#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include <filesys/file.h>
#include "synch.h"
#include <hash.h>
#include <filesys/directory.h>

/*! States in a thread's life cycle. */
enum thread_status {
    THREAD_RUNNING,     /*!< Running thread. */
    THREAD_READY,       /*!< Not running but ready to run. */
    THREAD_BLOCKED,     /*!< Waiting for an event to trigger. */
    THREAD_DYING        /*!< About to be destroyed. */
};

/*! Thread identifier type.
    You can redefine this to whatever type you like. */
typedef int tid_t;
typedef int pid_t;
#define TID_ERROR ((tid_t) -1)          /*!< Error value for tid_t. */

typedef int mapid_t;
#define MAP_FAIL ((mapid_t) -1)

/* Thread priorities. */
#define PRI_MIN 0                       /*!< Lowest priority. */
#define PRI_DEFAULT 31                  /*!< Default priority. */
#define PRI_MAX 63                      /*!< Highest priority. */
#define THREAD_MAX_STACK 2047


/*! A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

\verbatim
        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+
\endverbatim

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion.

   The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list.
*/

enum thread_type {
    THREAD_KERNEL,
    THREAD_PROCESS,
};

struct thread_return_status {
    pid_t pid;
    struct semaphore sem;
    struct semaphore exec_sem;
    int stat;
    struct list_elem elem;
    int load_success;                  /*!< Indicate whether executable file 
                                             is successfully loaded */
};

struct thread {
    /*! Owned by thread.c. */
    /**@{*/
    tid_t tid;                          /*!< Thread identifier. */
    enum thread_status status;          /*!< Thread state. */
    char name[16];                      /*!< Name (for debugging purposes). */
    uint8_t *stack;                     /*!< Saved stack pointer. */
    int nice;                           /*!< Niceness of the thread.*/
    int32_t recent_cpu;                     /*!< Recent_cpu of the thread */
    int priority;                       /*!< Intrinsic Priority. */
    int donated_priority;               /*!< Priority donated by other threads.*/
    struct list_elem allelem;           /*!< List element for all threads list. */
    struct list_elem elem;              /*!< List element */
    /**@}*/

    /*! Shared between thread.c and synch.c. */
    /**@{*/
    struct lock * waiting_lock;         /*!< The lock that the thread is waiting on. */

    /**@}*/

    int64_t wakeup_time;                /*!< Wake up time of the thread */

    struct list locks;                  /*!< List of locks acquired by the thread */
    struct dir * cur_dir;
#ifdef USERPROG
    /*! Owned by userprog/process.c. */
    /**@{*/
    uint32_t *pagedir;                  /*!< Page directory. */
    struct hash s_table;                /*! Supplemental page table*/
    uint32_t stack_no;                  /*! total number of stack of PGSIZE allocated */
    struct list mmap_lst;               /*! The list for mmap structs */
    mapid_t mmapid_max;
    bool syscall;
    void * esp;
    /**@{*/
    struct list child_returnstats;      /*!< List of child process return-stats */
    struct thread_return_status *trs;   /*!< Return-stats of this thread */
    struct list child_processes;        /*!< List of child processes */
    struct list_elem child_elem;        /*!< List element as parent's child */
    struct thread * parent;             /*!< Parent thread */
    struct list f_lst;                  /*!< List of opened files */
    uint32_t f_count;                   /*!< Number of opened files */
    uint32_t fd_max;                    /*!< Current largest file-descriptor number */
    enum thread_type type;              /*!< PROCESS or KERNEL */
    struct file* f_exe;                 /*!< Currently opened executable file */
    bool orphan;                        /*!< Whether parent has perished */

#endif
    /*! Owned by thread.c. */
    /**@{*/
    unsigned magic;                     /* Detects stack overflow. */
    /**@}*/
};

/*! The file info struct for each file accessed by a process */
struct f_info {
    /* The file object */
    struct file* f;
    /* The position of current access */
    off_t pos;
    /* List element for the list of all files of a process */
    struct list_elem elem;
    /* file descriptor */
    uint32_t fd;
    
    struct dir* d;
    
    bool isdir;
    
};

/*! If false (default), use round-robin scheduler.
    If true, use multi-level feedback queue scheduler.
    Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
#ifdef USERPROG
tid_t thread_create2(const char *name, int priority, thread_func *, void *,
                     enum thread_type type);
#endif
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);
void thread_unblock(struct thread *);

struct thread *thread_current (void);
tid_t thread_tid(void);
const char *thread_name(void);

struct thread_return_status *thread_findchild(pid_t pid);

void thread_exit(void) NO_RETURN;
void thread_yield(void);

bool thread_prioritycomp(const struct list_elem *a, const struct list_elem *b, 
                         void *aux);

void thread_refund_priority(void);
void thread_update_locks(struct thread *t, int nest_level);

/*! Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func(struct thread *t, void *aux);

void thread_foreach(thread_action_func *, void *);

int thread_get_priority(void);
void thread_set_priority(int);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);
static void thread_update_recent_cpu(struct thread* t, void *args UNUSED);
static void update_load_avg(void);
static void thread_update_priority(struct thread* t, void *args UNUSED);


#endif /* threads/thread.h */

