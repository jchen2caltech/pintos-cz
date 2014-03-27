#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <list.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/syscall.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "vm/frame.h"
#include "vm/page.h"

static thread_func start_process NO_RETURN;
static bool load(const char *cmdline, void (**eip)(void), void **esp);
static bool arg_pass(const char *cmdline, void **esp);
static bool push4(char** stack_ptr, void* val, void** esp);
static void get_prog_name(const char* cmdline, char* prog_name);

/*! Starts a new thread running a user program loaded from FILENAME.  The new
    thread may be scheduled (and may even exit) before process_execute()
    returns.  Returns the new process's thread id, or TID_ERROR if the thread
    cannot be created. */
tid_t process_execute(const char *file_name) {
    char *fn_copy;
    struct thread_return_status *trs;
    char prog_name[16];
    tid_t tid;
    /* Make a copy of FILE_NAME.
       Otherwise there's a race between the caller and load(). */
    fn_copy = palloc_get_page(0);
    if (fn_copy == NULL)
        return TID_ERROR;
    strlcpy(fn_copy, file_name, PGSIZE);
   
    /* Varaibles to extract the program name */
    get_prog_name(fn_copy, prog_name);

    /* Create a new thread to execute FILE_NAME. */
    tid = thread_create2(prog_name, PRI_DEFAULT, start_process, fn_copy, 
                         THREAD_PROCESS);
    trs = thread_findchild(tid);
    if (tid == TID_ERROR)
        palloc_free_page(fn_copy); 
    else 
        sema_down(&trs->exec_sem);

    if (tid != TID_ERROR && trs->load_success == -1)
        tid = TID_ERROR;
    return tid;
}

/*! A thread function that loads a user process and starts it running. */
static void start_process(void *file_name_) {
    
    char *file_name = file_name_;
    struct intr_frame if_;
    bool success;
    struct thread *cur;

    /* Initialize interrupt frame and load executable. */
    memset(&if_, 0, sizeof(if_));
    if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
    if_.cs = SEL_UCSEG;
    if_.eflags = FLAG_IF | FLAG_MBS;
    success = load(file_name, &if_.eip, &if_.esp);

    /* If load failed, quit. */
    palloc_free_page(file_name);
    cur = thread_current();
    if (success) {
        cur->trs->load_success = 0;
        sema_up(&cur->trs->exec_sem);
    }
    else {
        cur->trs->load_success = -1;
        sema_up(&cur->trs->exec_sem);
        thread_exit();
    }

    /* Start the user process by simulating a return from an
       interrupt, implemented by intr_exit (in
       threads/intr-stubs.S).  Because intr_exit takes all of its
       arguments on the stack in the form of a `struct intr_frame',
       we just point the stack pointer (%esp) to our stack frame
       and jump to it. */
    asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
    NOT_REACHED();
}

/*! Waits for thread TID to die and returns its exit status.  If it was
    terminated by the kernel (i.e. killed due to an exception), returns -1.
    If TID is invalid or if it was not a child of the calling process, or if
    process_wait() has already been successfully called for the given TID,
    returns -1 immediately, without waiting.
    Once returned from sema_down(), remove the trs from thread list and
    free the trs struct.
 */
int process_wait(tid_t child_tid) {
    struct thread *cur;
    struct thread_return_status *trs;
    enum intr_level old_level;
    int status;

    trs = thread_findchild(child_tid);
    cur = thread_current();
    if (!trs)
        return -1;
    old_level = intr_disable();
    sema_down(&trs->sem);
    status = trs->stat;
    list_remove(&trs->elem);
    free(trs);
    intr_set_level(old_level);
    return status;
}

/*! Free the current process's resources, signal its parent process if there
    is any, and put its return status into the thread_return_status if it
    it not an orphan. 
 */
void process_exit(void) {
    struct thread_return_status *trs;
    struct thread *cur = thread_current();
    struct thread *ct;
    uint32_t *pd;
    enum intr_level old_level;
    struct list_elem *ce;
    struct mmap_elem *cm;

    /* Destroy the current process's page directory and switch back
       to the kernel-only page directory. */
    trs = NULL;
    if (cur->parent) {
        old_level = intr_disable();
        cur->trs->stat = -1;
        intr_set_level(old_level);
    }
    /* Print exit message if and only if it is a USER-PROCESS */
    if (cur->type == THREAD_PROCESS)
        printf("%s: exit(%d)\n", cur->name, cur->trs->stat);

    /* Signal parent if there is any; otherwise free the return_status */
    if (!cur->orphan) {
        sema_up(&cur->trs->sem);
        list_remove(&cur->child_elem);
    } else {
        free(cur->trs);
    }
    /* Tell all the childs that they have become orphans */
    old_level = intr_disable();
    while (!list_empty(&cur->child_processes)) {
        ct = list_entry(list_pop_front(&cur->child_processes), struct thread,
                        child_elem);
        ct->orphan = true;
    }
    /* Free any unretrieved thread_return_status from childs */
    while (!list_empty(&cur->child_returnstats)) {
        trs = list_entry(list_pop_front(&cur->child_returnstats), 
                        struct thread_return_status, elem);
        free(trs);
    }
    
    intr_set_level(old_level);
    while (!list_empty(&cur->mmap_lst)) {
        ce = list_begin(&(cur->mmap_lst));
        cm = list_entry(ce, struct mmap_elem, elem);
        munmap(cm->mapid);
    }
    /*printf("hash_destroy\n");*/
    lock_acquire(&f_table.lock);
    hash_destroy(&cur->s_table, spte_destructor_func);
    lock_release(&f_table.lock);
    /* Close and allow write on executable file if any is opened */
    if (cur->f_exe){
        file_allow_write(cur->f_exe);
        file_close(cur->f_exe);
        cur->f_exe = NULL;
    }
    pd = cur->pagedir;
    if (pd != NULL) {
        /* Correct ordering here is crucial.  We must set
           cur->pagedir to NULL before switching page directories,
           so that a timer interrupt can't switch back to the
           process page directory.  We must activate the base page
           directory before destroying the process's page
           directory, or our active page directory will be one
           that's been freed (and cleared). */
        cur->pagedir = NULL;
        pagedir_activate(NULL);
        pagedir_destroy(pd);

    }
}

/*! Sets up the CPU for running user code in the current thread.
    This function is called on every context switch. */
void process_activate(void) {
    struct thread *t = thread_current();

    /* Activate thread's page tables. */
    pagedir_activate(t->pagedir);

    /* Set thread's kernel stack for use in processing interrupts. */
    tss_update();
}

/*! We load ELF binaries.  The following definitions are taken
    from the ELF specification, [ELF1], more-or-less verbatim.  */

/*! ELF types.  See [ELF1] 1-2. @{ */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;
/*! @} */

/*! For use with ELF types in printf(). @{ */
#define PE32Wx PRIx32   /*!< Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /*!< Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /*!< Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /*!< Print Elf32_Half in hexadecimal. */
/*! @} */

/*! Executable header.  See [ELF1] 1-4 to 1-8.
    This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
};

/*! Program header.  See [ELF1] 2-2 to 2-4.  There are e_phnum of these,
    starting at file offset e_phoff (see [ELF1] 1-6). */
struct Elf32_Phdr {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
};

/*! Values for p_type.  See [ELF1] 2-3. @{ */
#define PT_NULL    0            /*!< Ignore. */
#define PT_LOAD    1            /*!< Loadable segment. */
#define PT_DYNAMIC 2            /*!< Dynamic linking info. */
#define PT_INTERP  3            /*!< Name of dynamic loader. */
#define PT_NOTE    4            /*!< Auxiliary info. */
#define PT_SHLIB   5            /*!< Reserved. */
#define PT_PHDR    6            /*!< Program header table. */
#define PT_STACK   0x6474e551   /*!< Stack segment. */
/*! @} */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. @{ */
#define PF_X 1          /*!< Executable. */
#define PF_W 2          /*!< Writable. */
#define PF_R 4          /*!< Readable. */
/*! @} */

static bool setup_stack(void **esp);
static bool validate_segment(const struct Elf32_Phdr *, struct file *);
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
                         uint32_t read_bytes, uint32_t zero_bytes,
                         bool writable);

/*! Loads an ELF executable from FILE_NAME into the current thread.  Stores the
    executable's entry point into *EIP and its initial stack pointer into *ESP.
    Returns true if successful, false otherwise. */
bool load(const char *cmdline, void (**eip) (void), void **esp) {
    struct thread *t = thread_current();
    struct Elf32_Ehdr ehdr;
    struct file *file = NULL;
    off_t file_ofs;
    bool success = false;
    int i;
    char prog_name[16];

    /* Allocate and activate page directory. */
    t->pagedir = pagedir_create();
    if (t->pagedir == NULL) 
        goto done;
    process_activate();
    
    get_prog_name(cmdline, prog_name);
    
    /* Open executable file. */
    file = filesys_open(prog_name);
    
    if (file == NULL) {
        printf("load: %s: open failed\n", prog_name);
        goto done; 
    }
    t->f_exe = file;
    file_deny_write(file);

    /* Read and verify executable header. */
    if (file_read(file, &ehdr, sizeof ehdr) != sizeof ehdr ||
        memcmp(ehdr.e_ident, "\177ELF\1\1\1", 7) || ehdr.e_type != 2 ||
        ehdr.e_machine != 3 || ehdr.e_version != 1 ||
        ehdr.e_phentsize != sizeof(struct Elf32_Phdr) || ehdr.e_phnum > 1024) {
        printf("load: %s: error loading executable\n", prog_name);
        goto done; 
    }
    /* Read program headers. */
    file_ofs = ehdr.e_phoff;
    for (i = 0; i < ehdr.e_phnum; i++) {
        struct Elf32_Phdr phdr;

        if (file_ofs < 0 || file_ofs > file_length(file))
            goto done;
        file_seek(file, file_ofs);

        if (file_read(file, &phdr, sizeof phdr) != sizeof phdr)
            goto done;

        file_ofs += sizeof phdr;

        switch (phdr.p_type) {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
            /* Ignore this segment. */
            break;

        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
            goto done;

        case PT_LOAD:
            if (validate_segment(&phdr, file)) {
                bool writable = (phdr.p_flags & PF_W) != 0;
                uint32_t file_page = phdr.p_offset & ~PGMASK;
                uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
                uint32_t page_offset = phdr.p_vaddr & PGMASK;
                uint32_t read_bytes, zero_bytes;
                if (phdr.p_filesz > 0) {
                    /* Normal segment.
                       Read initial part from disk and zero the rest. */
                    read_bytes = page_offset + phdr.p_filesz;
                    zero_bytes = (ROUND_UP(page_offset + phdr.p_memsz, PGSIZE) -
                                 read_bytes);
                }
                else {
                    /* Entirely zero.
                       Don't read anything from disk. */
                    read_bytes = 0;
                    zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE);
                }
                if (!load_segment(file, file_page, (void *) mem_page,
                                  read_bytes, zero_bytes, writable))
                    goto done;
            }
            else {
                goto done;
            }
            break;
        }
    }
    
    /* Set up stack. */
    if (!setup_stack(esp))
        goto done;
    
    /* Start address. */
    *eip = (void (*)(void)) ehdr.e_entry;
    /*Passng Arguments*/
    if (!arg_pass(cmdline, esp))
        goto done;

    success = true;

done:
    /* We arrive here whether the load is successful or not. */
    return success;
}



/*! Checks whether PHDR describes a valid, loadable segment in
    FILE and returns true if so, false otherwise. */
static bool validate_segment(const struct Elf32_Phdr *phdr, struct file *file) {
    /* p_offset and p_vaddr must have the same page offset. */
    if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
        return false; 

    /* p_offset must point within FILE. */
    if (phdr->p_offset > (Elf32_Off) file_length(file))
        return false;

    /* p_memsz must be at least as big as p_filesz. */
    if (phdr->p_memsz < phdr->p_filesz)
        return false; 

    /* The segment must not be empty. */
    if (phdr->p_memsz == 0)
        return false;
  
    /* The virtual memory region must both start and end within the
       user address space range. */
    if (!is_user_vaddr((void *) phdr->p_vaddr))
        return false;
    if (!is_user_vaddr((void *) (phdr->p_vaddr + phdr->p_memsz)))
        return false;

    /* The region cannot "wrap around" across the kernel virtual
       address space. */
    if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
        return false;

    /* Disallow mapping page 0.
       Not only is it a bad idea to map page 0, but if we allowed it then user
       code that passed a null pointer to system calls could quite likely panic
       the kernel by way of null pointer assertions in memcpy(), etc. */
    if (phdr->p_vaddr < PGSIZE)
        return false;

    /* It's okay. */
    return true;
}

/*! Loads a segment starting at offset OFS in FILE at address UPAGE.  In total,
    READ_BYTES + ZERO_BYTES bytes of virtual memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

    The pages initialized by this function must be writable by the user process
    if WRITABLE is true, read-only otherwise.

    Return true if successful, false if a memory allocation error or disk read
    error occurs. */
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
                         uint32_t read_bytes, uint32_t zero_bytes,
                         bool writable) {
    
    ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT(pg_ofs(upage) == 0);
    ASSERT(ofs % PGSIZE == 0);
    
    while (read_bytes > 0 || zero_bytes > 0) {
        /* Calculate how to fill this page.
           We will read PAGE_READ_BYTES bytes from FILE
           and zero the final PAGE_ZERO_BYTES bytes. */
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;
        
        /* Create a supplemental page table for this page. Lazy loading...*/
        create_supp_table(file, ofs, upage, page_read_bytes, 
                               page_zero_bytes, writable);

        /* Update remaining read_bytes and zero_bytes;
         * Update upage and ofs for the next page. */
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        upage += PGSIZE;
        ofs += page_read_bytes;
    }
    return true;
}

/*! Create a minimal stack by mapping a zeroed page at the top of
    user virtual memory. */
static bool setup_stack(void **esp) {
    
    struct frame_table_entry *fr;
    struct supp_table * st;
    bool success = false;
    
    ASSERT(thread_current()->stack_no == 0);
    /* Create a new stack supplemental page table. */
    st = create_stack_supp_table(((uint8_t*) PHYS_BASE) - PGSIZE);
    
    /* Directly obtain the frame, as this is a the first stack for this 
     * process. Also set up the supplemental page table's frame, and
     * the number of stacks in this process is updated as 1 */
    fr = obtain_frame(PAL_USER | PAL_ZERO, st);
    st->fr = fr;
    thread_current()->stack_no = 1;
    
    /* Install the page.*/
    success = install_page(((uint8_t *) PHYS_BASE) - PGSIZE, 
                           fr->physical_addr, true);
    
    /* Setup the stack pointer. */
    if (success)
        *esp = PHYS_BASE ;
    return success;
}

/*! Adds a mapping from user virtual address UPAGE to kernel
    virtual address KPAGE to the page table.
    If WRITABLE is true, the user process may modify the page;
    otherwise, it is read-only.
    UPAGE must not already be mapped.
    KPAGE should probably be a page obtained from the user pool
    with palloc_get_page().
    Returns true on success, false if UPAGE is already mapped or
    if memory allocation fails. */

bool install_page(void *upage, void *kpage, bool writable) {
    struct thread *t = thread_current();

    /* Verify that there's not already a page at that virtual
       address, then map our page there. */
    return (pagedir_get_page(t->pagedir, upage) == NULL &&
            pagedir_set_page(t->pagedir, upage, kpage, writable));
}

/*! Parses the arguments into tokens;
    then pushes argv, argc, and fake return address in reverse order */
static bool arg_pass(const char*cmdline, void **esp) {
   /* Pointer to top of the stack */
   char* stack_top = *esp;
   
   /* Variables for argument tokenization */
   char* delimiters = " ";
   char* curr;
   char* word_begin;
   char* word_end;
   size_t word_len;
   
   /* Set the pointer to the end of cmdline, as we are scanning
      from the back */
   curr = cmdline + strlen(cmdline);
   while(curr >= cmdline)
   {
       /* If the current curr is not the delimiter then curr-- */
       while(curr>=cmdline && (strrchr(delimiters, *curr) != NULL 
           || *curr =='\0'))
           curr--;
       /* As we found the delimiter, word_end is cur + 1 */
       word_end = curr + 1;
       
       /* If we found another delimeter, then cur + 1 is word_begin */
       while(curr>=cmdline && strrchr(delimiters, *curr) == NULL)
           curr--;
       word_begin = curr + 1;
       
       /* Get word length */
       word_len = word_end - word_begin;
       
       /* Push the word onto stack */
       if ((int)*esp - (int)stack_top + word_len + 1 > PGSIZE)
           return false;
       strlcpy(stack_top - word_len - 1, word_begin, word_len + 1);
       
       /* Add null terminator */
       *(stack_top - 1) = '\0';
       
       /* Update stack_top */
       stack_top -= (word_len + 1);
       
   }
 
   char* p_argv_begin = stack_top;
   int count_limit;
   
   /* Add word align */
   if ((int)stack_top % 4 == 3) {
       count_limit = 1; 
   } else if ((int)stack_top % 4 == 0) {
       count_limit = 0;
   } else if ((int)stack_top % 4 == 1) {
       count_limit = 3;    
   } else {
       count_limit = 2;   
   }
   while(count_limit > 0){
       if ((int)*esp - (int)stack_top + 1 > PGSIZE)
           return false;
       stack_top--;
       *stack_top = 0;
       
       if(((int)stack_top)%4 == 0)
           count_limit--;
   }
   if (!push4(&stack_top, NULL, esp))
       return false;
   
   /* Scan from the base of the stack, to find the address of each arg
      Push the address to stack, then increment argc */
   char *p = PHYS_BASE - 1;
   int argc = 0;
   while (p>=p_argv_begin){
      p--;
      while ((p>=p_argv_begin) && (*p!='\0'))
          p--;
      if (!push4(&stack_top, (void*)(p+1), esp))
          return false;
      
      argc++;
       
   }
   
   /* Push the address of argv */
   if (!push4(&stack_top, (void*)(stack_top), esp))
       return false;
   
   /* Push argc */
   if (!push4(&stack_top, (void*)(argc), esp))
       return false;
   
   /* Push fake return address */
   if (!push4(&stack_top, NULL, esp))
       return false;
   
   /* Update esp */
   *esp = stack_top;
   return true;
}

/*! Push 4 bytes from val to the stack */
static bool push4(char** stack_ptr, void* val, void** esp) {
    /* First check whether pushing 4 bytes will overflow */
    if ((int)*esp - (int)*stack_ptr + 4 > PGSIZE)
        return false;
    
    /* Update stack pointer */
    *stack_ptr -= 4;
    /* Push value to stack */
    *((void **)(*stack_ptr)) = val;
    return true;
}

/*! Give a cmdline, return the program name */
static void get_prog_name(const char* cmdline, char* prog_name){
    /* First the first occurence of space */
    char* first_space = strchr(cmdline, ' ');
    
    if (first_space != NULL)
        /* If found space, then copy all chars before the first occurence */
        strlcpy(prog_name, cmdline, first_space - cmdline + 1);
    else
        /* Otherwise, the entire cmdline is prog_name */
        strlcpy(prog_name, cmdline, strlen(cmdline) + 1);
}
