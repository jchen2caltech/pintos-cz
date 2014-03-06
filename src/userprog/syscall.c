#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"


static void syscall_handler(struct intr_frame *);
bool checkva(const void* va);
struct f_info *findfile(uint32_t fd);
static uint32_t read4(struct intr_frame * f, int offset);
static struct lock filesys_lock;


void syscall_init(void) {
    lock_init(&filesys_lock);
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/*! Handler for each syscall. It reads the syscall-number from user stack
    and calls the corresponding syscall function accordingly. */

static void syscall_handler(struct intr_frame *f) {
    
    int status;
    const char *cmdline, *f_name;
    unsigned f_size, position, size;
    uint32_t fd;
    mapid_t mapping;
    struct supp_table *st;

    /* Retrieve syscall number */
    uint32_t sys_no = read4(f, 0);
    
    void *buffer; 
    pid_t pid;
    

    
    switch (sys_no) {
        case SYS_HALT:
            halt();
            break;
            
        case SYS_EXIT:
            status = (int) read4(f, 4);
            exit(status);
            break;
            
        case SYS_EXEC:
            cmdline = (const char*) read4(f, 4);
            f->eax = (uint32_t) exec(cmdline);
            break;
            
        case SYS_WAIT:
            pid = (pid_t) read4(f, 4);
            f->eax = (uint32_t) wait(pid);
            break;
            
        case SYS_CREATE:
            f_name = (const char*) read4(f, 4);
            f_size = (unsigned) read4(f, 8);
            f->eax = (uint32_t) create(f_name, f_size);
            break;
            
        case SYS_REMOVE:
            f_name = (const char*) read4(f, 4);
            f->eax = (uint32_t) remove(f_name);
            break;
            
        case SYS_OPEN:
            f_name = (const char*) read4(f, 4);
            f->eax = (uint32_t) open(f_name);
            break;
            
        case SYS_FILESIZE:
            fd = (uint32_t) read4(f, 4);
            f->eax = (uint32_t) filesize(fd);
            break;
            
        case SYS_READ:
            fd = (uint32_t) read4(f, 4);
            buffer = (void*) read4(f, 8);
            if ((uint32_t)f->esp - 32 <= (uint32_t)buffer && 
                (uint32_t)buffer >= PHYS_BASE - THREAD_MAX_STACK * PGSIZE &&
                !pagedir_get_page(thread_current()->pagedir, buffer)) {
                st = create_stack_supp_table(pg_round_down(buffer));
                st->fr = obtain_frame(PAL_USER | PAL_ZERO, st);
                ++ thread_current()->stack_no;
                if (!install_page(st->upage, st->fr->physical_addr, 
                    st->writable))
                    exit(-1);
            }
            size = (unsigned) read4(f, 12);
            f->eax = (uint32_t) read(fd, buffer, size);
            break;
            
        case SYS_WRITE:
            fd = (uint32_t) read4(f, 4);
            buffer = (void*) read4(f, 8);
            if ((uint32_t)f->esp - 32 <= (uint32_t)buffer && 
                (uint32_t)buffer >= PHYS_BASE - THREAD_MAX_STACK * PGSIZE &&
                !pagedir_get_page(thread_current()->pagedir, buffer)) {
                st = create_stack_supp_table(pg_round_down(buffer));
                st->fr = obtain_frame(PAL_USER | PAL_ZERO, st);
                ++ thread_current()->stack_no;
                if (!install_page(st->upage, st->fr->physical_addr, 
                    st->writable))
                    exit(-1);
            }
            size = (unsigned) read4(f, 12);
            f->eax = (uint32_t) write(fd, buffer, size);
            break;
            
        case SYS_SEEK:
            fd = (uint32_t) read4(f, 4);
            position = (unsigned) read4(f, 8);
            seek(fd, position);
            break;
            
        case SYS_TELL:
            fd = (uint32_t) read4(f, 4);
            f->eax = (uint32_t) tell(fd);
            break;
            
        case SYS_CLOSE:
            fd = (uint32_t) read4(f, 4);
            close(fd);
            break;

        case SYS_MMAP:
            fd = (uint32_t) read4(f, 4);
            buffer = (void*) read4(f, 8);
            f->eax = (uint32_t) mmap(fd, buffer);
            break;

        case SYS_MUNMAP:
            mapping = (mapid_t) read4(f, 4);
            munmap(mapping);
            break;

        default:
            exit(-1);
            break;
    }
    
}


/*! Reads for bytes from the stack according to the offset. */
static uint32_t read4(struct intr_frame * f, int offset) {
    if (!checkva(f->esp + offset))
        exit(-1);
    return *((uint32_t *) (f->esp + offset));
}

/*! halt */
void halt(void) {
    shutdown_power_off();
}

/*! exit */
void exit(int status) {
    struct thread *t;
    
    t = thread_current();
    t->trs->stat = status;
    t->parent = NULL;
    thread_exit();
}

/*! execute a command-line */
pid_t exec(const char *cmd_line) {
    if (checkva(cmd_line))
        return process_execute(cmd_line);
    exit(-1);
}

/*! wait for a child-process specified by pid, will return
    its return status (-1 if killed or failed) */
int wait(pid_t pid) {
    return process_wait(pid);
}

/*! Create a file */
bool create(const char *f_name, unsigned initial_size) {
    /* Checks the validity of the given pointer */
    if (!checkva(f_name))
        exit(-1);
    
    /* Create the file, while locking the file system. */
    lock_acquire(&filesys_lock);
    bool flag = filesys_create(f_name, (off_t) initial_size);
    lock_release(&filesys_lock);


    return flag;

}

/*! Remove a file */
bool remove(const char *f_name) {
    /* Checks the validity of the given pointer */
    if (!checkva(f_name))
        exit(-1);
    
    /* Remove the file, while locking the file system. */
    lock_acquire(&filesys_lock);
    bool flag = filesys_remove(f_name);
    lock_release(&filesys_lock);
    return flag;
}

/*! Open a file */
int open(const char *f_name) {
    struct thread *t;
    struct f_info *f;
    uint32_t fd;

    /* Checks the validity of the given pointer */
    if (!checkva(f_name))
       exit(-1);
    
    /* Open the file when locking the file system. */
    lock_acquire(&filesys_lock);
    struct file* f_open = filesys_open(f_name);
    lock_release(&filesys_lock);
    
    if (f_open == NULL) {
        /* If file open failed, then exit with error. */
        return -1;
    } else {
        t = thread_current();
        if (t->f_count > 127)
            exit(-1);
        
        /* Set up new f_info */
        f = (struct f_info*) malloc(sizeof(struct f_info));
        f->f = f_open;
        f->pos = 0;
        
        lock_acquire(&filesys_lock);
        fd = (++(t->fd_max));
        f->fd = fd;
        
        /* Push f_info to the thread's list, 
         * and update thread's list count. */
        list_push_back(&(t->f_lst), &(f->elem));
        ++(t->f_count);
        lock_release(&filesys_lock);
    }
    
    return fd;

}

/*! Get the file size give a fd. */
int filesize(uint32_t fd) {
    /* Find the fd in this process */
    struct f_info* f = findfile(fd);
    
    /* Find the size of the file */
    lock_acquire(&filesys_lock);
    int size = (int) file_length(f->f);
    lock_release(&filesys_lock);
    
    return size;

}

/*! Read from file */
int read(uint32_t fd, void *buffer, unsigned size) {
    /* Check the validity of given pointer */
    if ((!checkva(buffer)) || (!checkva(buffer + size))){
        /*printf("Bad Pointer:%x.\n", buffer);*/
        exit(-1);
    }
    
    int read_size = 0;
    if (fd == STDIN_FILENO) {
        /* If std-in, then read using input_getc() */
        unsigned i;
        for (i = 0; i < size; i++){
            *((uint8_t*) buffer) = input_getc();
            ++ read_size;
            ++ buffer;
        }
    } else {
        /* Otherwise, first find the file of this fd. */
        struct f_info* f = findfile(fd);
        struct file* fin = f->f;
        off_t pos = f->pos;
        
        /* Read from the file at f->pos */
        lock_acquire(&filesys_lock);
        read_size = (int) file_read_at(fin, buffer, (off_t) size, pos);
        f->pos += (off_t) read_size;
        lock_release(&filesys_lock);
        
    }
    return read_size;

}

/*! Write to file. */
int write(uint32_t fd, const void *buffer, unsigned size) {
    /* Check the validity of given pointer */
    if ((!checkva(buffer)) || (!checkva(buffer + size)))
        exit(-1);
    
    int write_size = 0;
    
    if (fd == STDOUT_FILENO) {
        /* If std-out, then write using putbuf() */
        putbuf(buffer, size);
        write_size = size;
        
    } else {
        /* Otherwise, first find the file of this fd. */
        struct f_info* f = findfile(fd);
        struct file* fout = f->f;
        off_t pos = f->pos;
        
        /* Write to the file at f->pos */
        lock_acquire(&filesys_lock);
        write_size = (int) file_write_at(fout, buffer, (off_t) size, pos);
        f->pos += (off_t) write_size;
        lock_release(&filesys_lock);
        
    }
    
    return write_size;

}

/*! Jump to a position of a file */
void seek(uint32_t fd, unsigned position) {
    /* first find the file of this fd. */
    struct f_info* f = findfile(fd);
    
    /* if position exceeds the file, then return the end of the file */
    if ((int) position > filesize(fd))
        position = (unsigned) filesize(fd);
    
    f->pos = (off_t) position;

}

/*! Return the position of the file the process is accessing*/
unsigned tell(uint32_t fd) {
    /* first find the file of this fd. */
    struct f_info* f = findfile(fd);
    
    return (unsigned) f->pos;
}

/*! Close an fd */
void close(uint32_t fd) {
    /* first find the file of this fd. */
    struct f_info* f = findfile(fd);
    
    /* Close the file, remove the f_info from the f_lst, and then
     * free this f_info. Update f_count accordingly. */
    
    lock_acquire(&filesys_lock);
    file_close(f->f);
    list_remove(&f->elem);
    free(f);
    struct thread* t = thread_current();
    --(t->f_count);
    lock_release(&filesys_lock);

}

/*!Checks whether the give user address va is inside the userprog stack
   and has been mapped. */

bool checkva(const void* va){
    struct thread *t = thread_current();
    /*return (is_user_vaddr(va) && (pagedir_get_page(t->pagedir, va) != NULL));*/
    return (is_user_vaddr(va) && va);
}

/*! Given a fd, then return the f_info struct in the current thread. */

struct f_info* findfile(uint32_t fd) {
    
    struct thread *t = thread_current();
    struct list* f_lst = &(t->f_lst);
    struct list_elem *e;
    
    /* Iterate through the entire f_lst to look for the same fd.
     * Return immediately, once found it. */
    for (e = list_begin(f_lst); e != list_end(f_lst); e = list_next(e)) {
        struct f_info* f = list_entry(e, struct f_info, elem);
        if (f->fd == fd)
            return f;
    }
    
    /* If not found, then exit with error. */
    exit(-1);
    return NULL;
    
}

mapid_t mmap(uint32_t fd, void* addr){
    int f_size;
    void* addr_e;
    mapid_t mapid;
    uint32_t read_bytes, zero_bytes;
    uint8_t *upage;
    off_t ofs;
    struct file* file;
    struct f_info* f;
    struct mmap_elem* me;
    struct supp_table* st;
    struct thread* t = thread_current();
    
    /*printf("Start mapping...\n");*/
    
    if (fd == STDIN_FILENO ||
        fd == STDOUT_FILENO ||
        filesize(fd) == 0 ||
        (!checkva(addr)) ||
        pg_ofs(addr) != 0 ||
        addr == 0) {
            return MAP_FAIL;
    }
    
    /*printf("where does it stuck?!");*/
    f_size = filesize(fd);
    for (addr_e = addr; addr_e < addr + f_size; addr_e += PGSIZE){
            if (find_supp_table(addr_e) != NULL){
                /*printf("Hello\n");*/
                return MAP_FAIL;
            }   
    }
    
    ++ t->mmapid_max;
    mapid = t->mmapid_max;
    me = (struct mmap_elem*) malloc(sizeof(struct mmap_elem));
    if (me == NULL)
        return MAP_FAIL;
    
    f = findfile(fd);
    
    /*lock_acquire(&filesys_lock);*/
    file = file_reopen(f->f);
    /*lock_release(&filesys_lock);*/
    
    if (file == NULL){
        free(me);
        return MAP_FAIL;
    }
    
    me->file = file;
    me->mapid = mapid;
    list_push_back(&(t->mmap_lst), &(me->elem));
    list_init(&(me->s_table));
    upage = addr;
    ofs = 0;
    read_bytes = f_size;
    
    if (read_bytes >= PGSIZE)
        zero_bytes = 0;
    else
        zero_bytes = PGSIZE - read_bytes;
    
    /*printf("Starting giving pages...\n");*/
    while (read_bytes > 0) {
        /* Calculate how to fill this page.
           We will read PAGE_READ_BYTES bytes from FILE
           and zero the final PAGE_ZERO_BYTES bytes. */
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;
        /*printf("Here? offset: %d at upage %x\n", ofs, upage);*/
        st = create_mmap_supp_table(file, ofs, upage, page_read_bytes, 
                                    page_zero_bytes, true);

        list_push_back(&(me->s_table), &(st->map_elem));
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        upage += PGSIZE;
        ofs += page_read_bytes;
    }
    /*printf("Mapping done!! %d\n", mapid);*/
    return mapid;
}


void munmap(mapid_t mapping){
    uint32_t f_size, write_bytes, page_write_size, pws2;
    off_t ofs = 0;
    struct list_elem *e;
    struct supp_table* st;
    struct mmap_elem *me = find_mmap_elem(mapping);
    struct thread* t = thread_current();
    
    printf("Unmapping %d\n", mapping);
    
    f_size = file_length(me->file);
    write_bytes = f_size;
    printf("1\n\n\n");
    for (e = list_begin(&(me->s_table)); e < list_end(&(me->s_table));
         e = list_next(e)) {
        st = list_entry(e, struct supp_table, map_elem);
        printf("2\n\n\n");
        if (st->fr) {
            printf("3\n\n\n");
            if (write_bytes >= PGSIZE)
                page_write_size = PGSIZE;
            else
                page_write_size = write_bytes;
            printf("4\n\n\n");
            if (pagedir_is_dirty(t->pagedir, st->upage)){
                printf("5\n\n\n");
                lock_acquire(&filesys_lock);
                printf("7\n\n\n");
                printf("file %x, upage %x, %d, %d", me->file, st->upage, page_write_size, ofs);
                pws2 = file_write_at(me->file, st->upage, 
                                                page_write_size, ofs);
                printf("8\n\n");
                lock_release(&filesys_lock);
                printf("6\n\n\n");
                ASSERT(pws2 == page_write_size);
            }
            ofs += page_write_size;
            write_bytes -= page_write_size;
        }
        spte_destructor_func(&(st->elem), NULL);
    }
    
    file_close(me->file);
    list_remove(&(me->elem));
    free(me);
}

struct mmap_elem* find_mmap_elem(mapid_t mapid){
    struct list_elem *e;
    struct thread* t = thread_current();
    struct list* m_lst = &(t->mmap_lst);
    struct mmap_elem* me;
    
    if (mapid == MAP_FAIL && mapid > t->mmapid_max) {
        printf("Mapid is out of range.\n");
        exit(-1);
    }
    
    for (e = list_begin(m_lst); e != list_end(m_lst); e = list_next(e)){
        me = list_entry(e, struct mmap_elem, elem);
        if (mapid == me->mapid)
            return me;
    }
    
    printf("Mapid is not found.\n");
    exit(-1);
    
}
