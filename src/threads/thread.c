#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include <hash.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/fixed-pt.h"
#include "devices/timer.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif
#include "filesys/file.h"
#include "filesys/directory.h"
#include "vm/frame.h"
#include "vm/page.h"

/*! Random value for struct thread's `magic' member.
    Used to detect stack overflow.  See the big comment at the top
    of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/*! List of processes in THREAD_READY state, that is, processes
    that are ready to run but not actually running. */
static struct list ready_list;

/*! List of all processes.  Processes are added to this list
    when they are first scheduled and removed when they exit. */
static struct list all_list;

/*! Idle thread. */
static struct thread *idle_thread;

/*! Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/*! Lock used by allocate_tid(). */
static struct lock tid_lock;


/*! Stack frame for kernel_thread(). */
struct kernel_thread_frame {
    void *eip;                  /*!< Return address. */
    thread_func *function;      /*!< Function to call. */
    void *aux;                  /*!< Auxiliary data for function. */
};

/* Statistics. */
static long long idle_ticks;    /*!< # of timer ticks spent idle. */
static long long kernel_ticks;  /*!< # of timer ticks in kernel threads. */
static long long user_ticks;    /*!< # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /*!< # of timer ticks to give each thread. */
static unsigned thread_ticks;   /*!< # of timer ticks since last yield. */

/*! If false (default), use round-robin scheduler.
    If true, use multi-level feedback queue scheduler.
    Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread(thread_func *, void *aux);

static void idle(void *aux UNUSED);
static struct thread *running_thread(void);
static struct thread *next_thread_to_run(void);
static void init_thread(struct thread *, const char *name, int priority, 
                        tid_t tid);
static bool is_thread(struct thread *) UNUSED;
static void *alloc_frame(struct thread *, size_t size);
static void schedule(void);
void thread_schedule_tail(struct thread *prev);
static tid_t allocate_tid(void);

/*! The global load average of the system*/
static int32_t load_avg;

/*! Initializes the threading system by transforming the code
    that's currently running into a thread.  This can't work in
    general and it is possible in this case only because loader.S
    was careful to put the bottom of the stack at a page boundary.

    Also initializes the run queue and the tid lock.

    After calling this function, be sure to initialize the page allocator
    before trying to create any threads with thread_create().

    It is not safe to call thread_current() until this function finishes. */
void thread_init(void) {
    ASSERT(intr_get_level() == INTR_OFF);
    lock_init(&tid_lock);
    list_init(&ready_list);
    list_init(&all_list);
    load_avg = 0;

    /* Set up a thread structure for the running thread. */
    initial_thread = running_thread();
    init_thread(initial_thread, "main", PRI_DEFAULT, 0);
    initial_thread->status = THREAD_RUNNING;
    initial_thread->tid = allocate_tid();
}

/*! Starts preemptive thread scheduling by enabling interrupts.
    Also creates the idle thread. */
void thread_start(void) {
    /* Create the idle thread. */
    struct semaphore idle_started;
    sema_init(&idle_started, 0);
    thread_create("idle", PRI_MIN, idle, &idle_started);

    /* Start preemptive thread scheduling. */
    intr_enable();

    /* Wait for the idle thread to initialize idle_thread. */
    sema_down(&idle_started);
}

/*! Called by the timer interrupt handler at each timer tick.
    Thus, this function runs in an external interrupt context. */
void thread_tick(void) {
    struct thread *t = thread_current();

    /* Update statistics. */
    if (t == idle_thread){
        idle_ticks++;
    } else {
        if (thread_mlfqs){
            t->recent_cpu = FADDI(t->recent_cpu, 1);
        }
#ifdef USERPROG
        
        if (t->pagedir != NULL)
            user_ticks++;
#endif
        else
            kernel_ticks++;
    }
    
    if (thread_mlfqs){
        if (timer_ticks() % TIMER_FREQ == 0){
            /* When time passed as multiple of a second, then update
            * load_avg and all thread's recent_cpu.*/
            update_load_avg();
        
            thread_foreach(thread_update_recent_cpu, NULL);
            
        }
        
        if (timer_ticks() % 4 == 0)
            /*For every fourth clock tick, update all priority.*/
            thread_foreach(thread_update_priority, NULL);
        
    }
    
    

    /* Enforce preemption. */
    if (++thread_ticks >= TIME_SLICE)
        intr_yield_on_return();
}

/*! Prints thread statistics. */
void thread_print_stats(void) {
    printf("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
           idle_ticks, kernel_ticks, user_ticks);
}

/*! Creates a new kernel thread named NAME with the given initial PRIORITY,
    which executes FUNCTION passing AUX as the argument, and adds it to the
    ready queue.  Returns the thread identifier for the new thread, or
    TID_ERROR if creation fails.

    If thread_start() has been called, then the new thread may be scheduled
    before thread_create() returns.  It could even exit before thread_create()
    returns.  Contrariwise, the original thread may run for any amount of time
    before the new thread is scheduled.  Use a semaphore or some other form of
    synchronization if you need to ensure ordering.

    The code provided sets the new thread's `priority' member to PRIORITY, but
    no actual priority scheduling is implemented.  Priority scheduling is the
    goal of Problem 1-3. */

tid_t thread_create(const char *name, int priority, thread_func *function,
                    void *aux) {
#ifdef USERPROG
    return thread_create2(name, priority, function, aux, THREAD_KERNEL);
}
/* A stub for creating USER-PROCESS thread */

tid_t thread_create2(const char *name, int priority, thread_func *function, 
                    void *aux, enum thread_type type) {
#endif

    struct thread *t;
    struct kernel_thread_frame *kf;
    struct switch_entry_frame *ef;
    struct switch_threads_frame *sf;
    tid_t tid;

    ASSERT(function != NULL);

    /* Allocate thread. */
    t = palloc_get_page(PAL_ZERO);
    if (t == NULL)
        return TID_ERROR;

    /* Initialize thread. */
    t->tid = allocate_tid();
    tid = t->tid;
    init_thread(t, name, priority, tid);
#ifdef USERPROG
    t->type = type;
    supp_table_init(&(t->s_table));
    list_init(&(t->mmap_lst));
    t->stack_no = 0;
    t->mmapid_max = 0;
    t->esp = NULL;
    t->syscall = false;
#endif
    /* Stack frame for kernel_thread(). */
    kf = alloc_frame(t, sizeof *kf);
    kf->eip = NULL;
    kf->function = function;
    kf->aux = aux;

    /* Stack frame for switch_entry(). */
    ef = alloc_frame(t, sizeof *ef);
    ef->eip = (void (*) (void)) kernel_thread;

    /* Stack frame for switch_threads(). */
    sf = alloc_frame(t, sizeof *sf);
    sf->eip = switch_entry;
    sf->ebp = 0;

    /* Add to run queue. */
    thread_unblock(t);
    
    if (priority > thread_get_priority())
        thread_yield();
    return tid;
}

/*! Puts the current thread to sleep.  It will not be scheduled
    again until awoken by thread_unblock().

    This function must be called with interrupts turned off.  It is usually a
    better idea to use one of the synchronization primitives in synch.h. */
void thread_block(void) {
    ASSERT(!intr_context());
    ASSERT(intr_get_level() == INTR_OFF);

    thread_current()->status = THREAD_BLOCKED;
    schedule();
}

/*! Transitions a blocked thread T to the ready-to-run state.  This is an
    error if T is not blocked.  (Use thread_yield() to make the running
    thread ready.)

    This function does not preempt the running thread.  This can be important:
    if the caller had disabled interrupts itself, it may expect that it can
    atomically unblock a thread and update other data. */
void thread_unblock(struct thread *t) {
    enum intr_level old_level;

    ASSERT(is_thread(t));

    old_level = intr_disable();
    ASSERT(t->status == THREAD_BLOCKED);
    list_push_back(&ready_list, &t->elem);
    t->status = THREAD_READY;
    intr_set_level(old_level);
}

/*! Returns the name of the running thread. */
const char * thread_name(void) {
    return thread_current()->name;
}

/*! Returns the running thread.
    This is running_thread() plus a couple of sanity checks.
    See the big comment at the top of thread.h for details. */
struct thread * thread_current(void) {
    struct thread *t = running_thread();

    /* Make sure T is really a thread.
       If either of these assertions fire, then your thread may
       have overflowed its stack.  Each thread has less than 4 kB
       of stack, so a few big automatic arrays or moderate
       recursion can cause stack overflow. */
    ASSERT(is_thread(t));
    ASSERT(t->status == THREAD_RUNNING);

    return t;
}

/*! Returns the running thread's tid. */
tid_t thread_tid(void) {
    return thread_current()->tid;
}
#ifdef USERPROG

/*! Finds the child process thread_return_status struct according to
    a thread pid. Will return NULL if the passed-in pid is not a
    running/recently exited child process of the current thread. */

struct thread_return_status *thread_findchild(pid_t pid) {
    struct thread *t, *ct;
    struct list_elem *ce;
    struct thread_return_status *trs = NULL;
    struct thread_return_status *cs;
    enum intr_level old_level;
    
    t = thread_current();
    old_level = intr_disable();
    ce = list_begin(&t->child_returnstats);
    while (ce->next && !trs) {
        cs = list_entry(ce, struct thread_return_status, elem);
        if (cs->pid == pid)
            trs = cs;
        ce = list_next(ce);
    }
    intr_set_level(old_level);
    return trs;
}
#endif
/*! Deschedules the current thread and destroys it.  Never
    returns to the caller. */
void thread_exit(void) {
    struct thread *t;
    struct list_elem *ce;
    struct f_info *cf;
    struct thread_return_status *ctrs;

    ASSERT(!intr_context());
    t = thread_current();
#ifdef USERPROG
    if (t->type == THREAD_PROCESS)
        process_exit();
    /* Free all remaining opened files */
    while (!list_empty(&t->f_lst)) {
        ce = list_pop_front(&t->f_lst);
        cf = list_entry(ce, struct f_info, elem);
        if (cf->isdir)
            dir_close(cf->d);
        else
            file_close(cf->f);
        free(cf);
    }
    /* Free all remaining child-returnstats */
    while (!list_empty(&t->child_returnstats)) {
        ce = list_pop_front(&t->child_returnstats);
        ctrs = list_entry(ce, struct thread_return_status, elem);
        free(ctrs);
    }
    if (t->cur_dir != NULL)
        dir_close(t->cur_dir);
#endif

    /* Remove thread from all threads list, set our status to dying,
       and schedule another process.  That process will destroy us
       when it calls thread_schedule_tail(). */
    intr_disable();
    /*ASSERT(list_empty(&thread_current()->locks));*/
    list_remove(&t->allelem);
    t->status = THREAD_DYING;
    schedule();
    NOT_REACHED();
}

/*! Yields the CPU.  The current thread is not put to sleep and
    may be scheduled again immediately at the scheduler's whim. */
void thread_yield(void) {
    struct thread *cur = thread_current();
    enum intr_level old_level;

    ASSERT(!intr_context());

    old_level = intr_disable();
    if (cur != idle_thread) 
        list_push_back(&ready_list, &cur->elem);
    cur->status = THREAD_READY;
    schedule();
    intr_set_level(old_level);
}

/*! Invoke function 'func' on all threads, passing along 'aux'.
    This function must be called with interrupts off. */
void thread_foreach(thread_action_func *func, void *aux) {
    struct list_elem *e;

    ASSERT(intr_get_level() == INTR_OFF);

    for (e = list_begin(&all_list); e != list_end(&all_list);
         e = list_next(e)) {
        struct thread *t = list_entry(e, struct thread, allelem);
        func(t, aux);
    }
}

/*! Sets the current thread's priority to NEW_PRIORITY. */
void thread_set_priority(int new_priority) {
    struct thread *hp;
    enum intr_level old_level;
    int old_priority;

    ASSERT(new_priority <= PRI_MAX && new_priority >= PRI_MIN);
    old_level = intr_disable();
    old_priority = thread_get_priority();
    thread_current()->priority = new_priority;
    if (thread_get_priority() < old_priority && !list_empty(&ready_list)) {
        hp = list_entry(list_max(&ready_list, thread_prioritycomp, NULL),
                        struct thread, elem);
        if (hp->priority > new_priority)
            thread_yield();
    }
    intr_set_level(old_level);
}

/*! Update the priority of the lock that a thread is waiting on. 
    This forms a nested call with lock_reset_priority() function, thus
    there is a limit to the recursion depth. */
void thread_update_locks(struct thread *t, int nest_level) {
    if (t->waiting_lock) {
        lock_reset_priority(t->waiting_lock, nest_level); 
    }
}

/*! Returns the current thread's priority. */
int thread_get_priority(void) {
    int p1, p2;

    p1 = thread_current()->priority;
    p2 = thread_current()->donated_priority;
    return (p1 < p2 ? p2 : p1);
}

/*! Update the priority of the input thread. This is part of the BSD
 *  Scheduler. */
static void thread_update_priority(struct thread* t, void* args UNUSED){
    int p;
    
    p = PRI_MAX - F2IN(FDIVI(t->recent_cpu, 4)) - (t->nice) * 2; 
    /* Clamp the priority p if > PRI_MAX or < PEI_MIN*/
    if (p > PRI_MAX)
        p = PRI_MAX;
    
    if (p < PRI_MIN)
        p = PRI_MIN;
    
    t->priority = p;
    
}

/*! Update the global variable load_avg*/
static void update_load_avg(void) {
    /* First get the number of ready threads*/
    size_t num_ready = list_size(&ready_list);
    if (thread_current()!=idle_thread 
          && thread_current()->status == THREAD_RUNNING)
        ++ num_ready;
    
    /*Update load_avg*/
    
    int32_t coeff = FDIVF(I2F(59), I2F(60));
    load_avg = FMULF(load_avg, coeff);
    coeff = FMULI(FDIVI(coeff, 59), num_ready);
    load_avg += coeff;
    
}

/*! Update the recent_cpu of the input thread.*/
static void thread_update_recent_cpu(struct thread* t, void* args UNUSED){    
    int32_t coeff1 = FMULI(load_avg, 2);
    int32_t coeff2 = FDIVF(coeff1, FADDI(coeff1, 1));
    t->recent_cpu = FADDI(FMULF(coeff2, t->recent_cpu), t->nice);
}

/*! Sets the current thread's nice value to NICE. */
void thread_set_nice(int nice UNUSED) {
    int old_p;
    
    thread_current()->nice = nice;
    /*Update thread->priority accordingly*/
    old_p = thread_current()->priority;
    thread_update_priority(thread_current(), NULL);
    /*If the current thread's priority is lowered, then yield to scheduler*/
    if (old_p > (thread_current()->priority))
        thread_yield();
}

/*! Returns the current thread's nice value. */
int thread_get_nice(void) {
    return thread_current()->nice;
}

/*! Returns 100 times the system load average. */
int thread_get_load_avg(void) {
    return F2IN(FMULI(load_avg, 100));
}

/*! Returns 100 times the current thread's recent_cpu value. */
int thread_get_recent_cpu(void) {
    return F2IN(FMULI(thread_current()->recent_cpu, 100));
}

/*! Idle thread.  Executes when no other thread is ready to run.

    The idle thread is initially put on the ready list by thread_start().
    It will be scheduled once initially, at which point it initializes
    idle_thread, "up"s the semaphore passed to it to enable thread_start()
    to continue, and immediately blocks.  After that, the idle thread never
    appears in the ready list.  It is returned by next_thread_to_run() as a
    special case when the ready list is empty. */
static void idle(void *idle_started_ UNUSED) {
    struct semaphore *idle_started = idle_started_;
    idle_thread = thread_current();
    sema_up(idle_started);

    for (;;) {
        /* Let someone else run. */
        intr_disable();
        thread_block();

        /* Re-enable interrupts and wait for the next one.

           The `sti' instruction disables interrupts until the completion of
           the next instruction, so these two instructions are executed
           atomically.  This atomicity is important; otherwise, an interrupt
           could be handled between re-enabling interrupts and waiting for the
           next one to occur, wasting as much as one clock tick worth of time.

           See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
           7.11.1 "HLT Instruction". */
        asm volatile ("sti; hlt" : : : "memory");
    }
}

/*! Function used as the basis for a kernel thread. */
static void kernel_thread(thread_func *function, void *aux) {
    ASSERT(function != NULL);

    intr_enable();       /* The scheduler runs with interrupts off. */
    function(aux);       /* Execute the thread function. */
    thread_exit();       /* If function() returns, kill the thread. */
}

/*! Returns the running thread. */
struct thread * running_thread(void) {
    uint32_t *esp;

    /* Copy the CPU's stack pointer into `esp', and then round that
       down to the start of a page.  Because `struct thread' is
       always at the beginning of a page and the stack pointer is
       somewhere in the middle, this locates the curent thread. */
    asm ("mov %%esp, %0" : "=g" (esp));
    return pg_round_down(esp);
}

/*! Returns true if T appears to point to a valid thread. */
static bool is_thread(struct thread *t) {
    return (t != NULL && t->magic == THREAD_MAGIC);
}

/*! Does basic initialization of T as a blocked thread named NAME. */
static void init_thread(struct thread *t, const char *name, int priority,
                        tid_t tid) {
    enum intr_level old_level;
    struct thread_return_status *trs;

    ASSERT(t != NULL);
    ASSERT(PRI_MIN <= priority && priority <= PRI_MAX);
    ASSERT(name != NULL);
    memset(t, 0, sizeof *t);
    t->status = THREAD_BLOCKED;
    strlcpy(t->name, name, sizeof t->name);
    t->stack = (uint8_t *) t + PGSIZE;
    t->tid = tid;
    
   /* Set Up priority according to the algorithm;*/
 
    if (thread_mlfqs) {
        if (t == initial_thread){
            t->nice = 0;
            t->recent_cpu = 0;
        } else {
            t->nice = thread_current()->nice;
            t->recent_cpu = thread_current()->recent_cpu;   
        }
        
        thread_update_priority(t, NULL);
        
    } else {
         t->priority = priority;
         t->donated_priority = PRI_MIN;
    }
    t->magic = THREAD_MAGIC;
    list_init(&t->locks); 
    t->waiting_lock = NULL;
    old_level = intr_disable();
#ifdef USERPROG

    list_init(&t->child_returnstats);
    if (t == initial_thread) {
        t->parent = NULL;
        t->cur_dir = NULL;
    } else {
        t->parent = thread_current();
        trs = malloc(sizeof(struct thread_return_status));
        trs->pid = (pid_t)t->tid;
        sema_init(&trs->sem, 0);
        sema_init(&trs->exec_sem, 0);
        t->trs = trs;
        list_push_back(&(thread_current()->child_returnstats), &trs->elem);
        list_push_back(&thread_current()->child_processes, &t->child_elem);
        list_init(&t->f_lst);
        t->f_count = 2;
        t->fd_max = 1;
        if (t->parent->cur_dir == NULL)
            t->cur_dir = NULL;
        else
            t->cur_dir = dir_reopen(t->parent->cur_dir);
    }
    list_init(&t->child_processes);
    t->f_exe = NULL;
    t->orphan = false;

#endif
    
    list_push_back(&all_list, &t->allelem);
    intr_set_level(old_level);
}

/*! Allocates a SIZE-byte frame at the top of thread T's stack and
    returns a pointer to the frame's base. */
static void * alloc_frame(struct thread *t, size_t size) {
    /* Stack data is always allocated in word-size units. */
    ASSERT(is_thread(t));
    ASSERT(size % sizeof(uint32_t) == 0);

    t->stack -= size;
    return t->stack;
}

/*! Chooses and returns the next thread to be scheduled.  Should return a
    thread from the run queue, unless the run queue is empty.  (If the running
    thread can continue running, then it will be in the run queue.)  If the
    run queue is empty, return idle_thread. */
static struct thread * next_thread_to_run(void) {
    struct thread * nt;
    enum intr_level old_level;

    if (list_empty(&ready_list)) {
        return idle_thread;
    }
    else {
        old_level = intr_disable();
        nt = list_entry(list_max(&ready_list, thread_prioritycomp, NULL), 
                        struct thread, elem);
        list_remove(&nt->elem);
        intr_set_level(old_level);
        return nt;
    }
}

/*! Completes a thread switch by activating the new thread's page tables, and,
    if the previous thread is dying, destroying it.

    At this function's invocation, we just switched from thread PREV, the new
    thread is already running, and interrupts are still disabled.  This
    function is normally invoked by thread_schedule() as its final action
    before returning, but the first time a thread is scheduled it is called by
    switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is complete.  In
   practice that means that printf()s should be added at the end of the
   function.

   After this function and its caller returns, the thread switch is complete. */
void thread_schedule_tail(struct thread *prev) {
    struct thread *cur = running_thread();
  
    ASSERT(intr_get_level() == INTR_OFF);

    /* Mark us as running. */
    cur->status = THREAD_RUNNING;

    /* Start new time slice. */
    thread_ticks = 0;

#ifdef USERPROG
    /* Activate the new address space. */
    process_activate();
#endif

    /* If the thread we switched from is dying, destroy its struct thread.
       This must happen late so that thread_exit() doesn't pull out the rug
       under itself.  (We don't free initial_thread because its memory was
       not obtained via palloc().) */
    if (prev != NULL && prev->status == THREAD_DYING &&
        prev != initial_thread) {
        ASSERT(prev != cur);
        palloc_free_page(prev);
    }
}

/*! Schedules a new process.  At entry, interrupts must be off and the running
    process's state must have been changed from running to some other state.
    This function finds another thread to run and switches to it.

    It's not safe to call printf() until thread_schedule_tail() has
    completed. */
static void schedule(void) {
    struct thread *cur = running_thread();
    struct thread *next = next_thread_to_run();
    struct thread *prev = NULL;

    ASSERT(intr_get_level() == INTR_OFF);
    ASSERT(cur->status != THREAD_RUNNING);
    ASSERT(is_thread(next));

    if (cur != next)
        prev = switch_threads(cur, next);
    thread_schedule_tail(prev);
}

/*! List_leff_func for priority scheduling. This function compares to list
    elements that belong to two threads based on the threads' priority. 
    Note that donated_priority is also taken into account. */

bool thread_prioritycomp(const struct list_elem *a, const struct list_elem *b,
                         void *aux) {
    enum intr_level old_level;
    int p1, p2;
    struct thread *t1, *t2;

    old_level = intr_disable();
    t1 = list_entry(a, struct thread, elem);
    t2 = list_entry(b, struct thread, elem);
    p1 = t1->priority;
    if (p1 < t1->donated_priority) {
        p1 = t1->donated_priority;
    }

    p2 = t2->priority;
    if (p2 < t2->donated_priority) {
        p2 = t2->donated_priority;
    }
    
    intr_set_level(old_level);

    return (p1 < p2);
}

/*! Reset the donated_priority of the thread. This function is called when
    a lock has been released, and the original owner is thus forced to
    refund the donated priority. It finds the new highest priority from
    the locks that it now owns. */

void thread_refund_priority(void) {
    struct lock *l;
    enum intr_level old_level;
    struct thread *ct;

    old_level = intr_disable();
    ct = thread_current();
    if (list_empty(&ct->locks)) {
        ct->donated_priority = PRI_MIN;
    }
    else {
        l = list_entry(list_max(&ct->locks, 
                                lock_prioritycomp, NULL),
                   struct lock, elem);
        ct->donated_priority = l->priority;
    }
    intr_set_level(old_level);
}     

/*! Returns a tid to use for a new thread. */
static tid_t allocate_tid(void) {
    static tid_t next_tid = 1;
    tid_t tid;

    lock_acquire(&tid_lock);
    tid = next_tid++;
    lock_release(&tid_lock);

    return tid;
}

/*! Offset of `stack' member within `struct thread'.
    Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof(struct thread, stack);

