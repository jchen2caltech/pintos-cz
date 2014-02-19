#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "devices/input.h"
#include "userprog/pagedir.h"

static void syscall_handler(struct intr_frame *);

void syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f UNUSED) {
    printf("system call!\n");
    thread_exit();
}

void halt(void) {
    shutdown_power_off();
}

void exit(int status) {

}

pid_t exec(const char *cmd_line) {

}

int wait(pid_t pid) {

}

bool create(const char *file, unsigned initial_size) {
    if (!checkva(file))
        exit(-1);
    // Probably Lock?!
    bool flag = filesys_create(file, initial_size);
    // Unlock?!
    return flag;

}

bool remove(const char *file) {
    if (!checkva(file))
        exit(-1);
    // Lock?!
    bool flag = filesys_create(file);
    // Unlock?!
    return flag;
    

}

int open(const char *file) {
    if (!checkva(file) || !(checkva(file + strlen(file))))
        exit(-1);
    
    //Lock?!
    struct file* f_open = filesys_open(file);
    //Unlock?!
    
    if (f_open == NULL) {
        return -1
    } else {
        struct thread* t = thread_current();
        if (t->f_count > 127)
            exit(-1);
        
        // Set up new f_info
        struct f_info* f = (struct f_info*) malloc(sizeof(f_info));
        f->f = f_open;
        f->pos = 0;
        
        //lock?!
        uint32_t fd = (++(t->fd_max));
        f->fd = fd;
        
        // Push f_info to the thread's list, and update thread's list count.
        list_push_back(&(t->f_lst), &(f->elem));
        ++(t->f_count);
        //unlock?!
    }
    
    return fd;

}

int filesize(uint32_t fd) {
    struct f_info* f = findfile(fd);
    //Lock?!
    int size = (int) file_length(f->f);
    //Unlock?!
    
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
        file* fin = f->f;
        off_t pos = f->pos;
        
        //Lock?!
        read_size = (int) file_read_at(fin, buffer, (off_t) size, pos);
        f->pos += (off_t) read_size;
        //Unlock?!
        
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
        file* fout = f->f;
        off_t pos = f->pos;
        
        //Lock?!
        write_size = (int) file_write_at(fout, buffer, (off_t) size, pos);
        f->pos += (off_t) write_size;
        //Unlock?!
        
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
    list_remove(f->elem);
    
    //lock?
    
    file_close(f->f);
    free(f);
    struct thread* t = thread_current();
    --(t->f_count);
    //unlock?

}

/*!Checks whether the give user address va is inside the userprog stack
   and has been mapped. */

bool checkva(const void* va){
    struct *t = thread_current();
    return (is_user_vaddr(va) && (pagedir_get_page(t->pagedir, va) != NULL));
}

/*! Given a fd, then return the f_info struct in the current thread. */

struct f_info* findfile(uint32_t fd) {
    
    struct *t = thread_current();
    struct list* f_lst = &(t->f_lst);
    struct list_elem *e;
    
    for (e = list_begin(f_lst); e != list_end(f_lst); e = lst_next(e)) {
        struct f_info* f = list_entry(e, struct f_info, elem);
        if (f->fd == fd)
            return f;
    }
    
    exit(-1);
    return NULL;
    
}