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

static struct lock filesys_lock;

static void syscall_handler(struct intr_frame *);
bool checkva(const void* va);
struct f_info *findfile(uint32_t fd);
static uint32_t read4(struct intr_frame * f, int offset);


void syscall_init(void) {
    lock_init(&filesys_lock);
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f) {
    // Retrieve syscall number
    int status;
    const char *cmdline, *f_name;
    unsigned f_size, position, size;
    uint32_t fd;
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
            size = (unsigned) read4(f, 12);
            f->eax = (uint32_t) read(fd, buffer, size);
            break;
            
        case SYS_WRITE:
            fd = (uint32_t) read4(f, 4);
            buffer = (void*) read4(f, 8);
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
            
        default:
            exit(-1);
            break;
    }
    
}

static uint32_t read4(struct intr_frame * f, int offset) {
    if (!checkva(f->esp + offset))
        exit(-1);
    return *((uint32_t *) (f->esp + offset));
}

void halt(void) {
    shutdown_power_off();
}

void exit(int status) {
    struct thread *t;

    t = thread_current();
    t->trs->stat = status;
    t->parent = NULL;
    thread_exit();
}

pid_t exec(const char *cmd_line) {
    if (checkva(cmd_line))
        return process_execute(cmd_line);
    exit(-1);
}

int wait(pid_t pid) {
    return process_wait(pid);
}

bool create(const char *f_name, unsigned initial_size) {
    if (!checkva(f_name))
        exit(-1);
    //printf("\n\nHello\n\n");
    lock_acquire(&filesys_lock);
    //printf("\n\ncreating file name:[%s]\n", f_name);
    //printf("And the size is %d\n\n\n", initial_size);
    bool flag = filesys_create(f_name, (off_t) initial_size);
    lock_release(&filesys_lock);


    return flag;

}

bool remove(const char *f_name) {
    if (!checkva(f_name))
        exit(-1);
    lock_acquire(&filesys_lock);
    bool flag = filesys_remove(f_name);
    lock_release(&filesys_lock);
    return flag;
}

int open(const char *f_name) {
    struct thread *t;
    struct f_info *f;
    uint32_t fd;

    if (!checkva(f_name))
       exit(-1);
    
    lock_acquire(&filesys_lock);
    struct file* f_open = filesys_open(f_name);
    lock_release(&filesys_lock);
    
    if (f_open == NULL) {
        return -1;
    } else {
        t = thread_current();
        if (t->f_count > 127)
            exit(-1);
        
        // Set up new f_info
        f = (struct f_info*) malloc(sizeof(struct f_info));
        f->f = f_open;
        f->pos = 0;
        
        lock_acquire(&filesys_lock);
        fd = (++(t->fd_max));
        f->fd = fd;
        
        // Push f_info to the thread's list, and update thread's list count.
        list_push_back(&(t->f_lst), &(f->elem));
        ++(t->f_count);
        lock_release(&filesys_lock);
    }
    
    return fd;

}

int filesize(uint32_t fd) {
    struct f_info* f = findfile(fd);
    lock_acquire(&filesys_lock);
    int size = (int) file_length(f->f);
    lock_release(&filesys_lock);
    
    return size;

}

int read(uint32_t fd, void *buffer, unsigned size) {
    if ((!checkva(buffer)) || (!checkva(buffer + size)))
        exit(-1);
    
    int read_size = 0;
    
    if (fd == STDIN_FILENO) {
        unsigned i;
        for (i = 0; i < size; i++){
            *((uint8_t*) buffer) = input_getc();
            ++ read_size;
            ++ buffer;
        }
    } else {
        struct f_info* f = findfile(fd);
        struct file* fin = f->f;
        off_t pos = f->pos;
        
        lock_acquire(&filesys_lock);
        read_size = (int) file_read_at(fin, buffer, (off_t) size, pos);
        f->pos += (off_t) read_size;
        lock_release(&filesys_lock);
        
    }
    
    return read_size;

}

int write(uint32_t fd, const void *buffer, unsigned size) {
    
    if ((!checkva(buffer)) || (!checkva(buffer + size)))
        exit(-1);
    
    int write_size = 0;
    
    if (fd == STDOUT_FILENO) {
        putbuf(buffer, size);
        write_size = size;
        
    } else {
        struct f_info* f = findfile(fd);
        struct file* fout = f->f;
        off_t pos = f->pos;
        
        lock_acquire(&filesys_lock);
        write_size = (int) file_write_at(fout, buffer, (off_t) size, pos);
        f->pos += (off_t) write_size;
        lock_release(&filesys_lock);
        
    }
    
    return write_size;

}

void seek(uint32_t fd, unsigned position) {
    struct f_info* f = findfile(fd);
    
    // if position exceeds the file, then return the end of the file
    if ((int) position > filesize(fd))
        position = (unsigned) filesize(fd);
    
    f->pos = (off_t) position;

}

unsigned tell(uint32_t fd) {
    struct f_info* f = findfile(fd);
    return (unsigned) f->pos;
}

void close(uint32_t fd) {
    struct f_info* f = findfile(fd);
    list_remove(&f->elem);
    
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
    return (is_user_vaddr(va) && (pagedir_get_page(t->pagedir, va) != NULL));
}

/*! Given a fd, then return the f_info struct in the current thread. */

struct f_info* findfile(uint32_t fd) {
    
    struct thread *t = thread_current();
    struct list* f_lst = &(t->f_lst);
    struct list_elem *e;
    
    for (e = list_begin(f_lst); e != list_end(f_lst); e = list_next(e)) {
        struct f_info* f = list_entry(e, struct f_info, elem);
        if (f->fd == fd)
            return f;
    }
    
    exit(-1);
    return NULL;
    
}
