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
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/free-map.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"


static void syscall_handler(struct intr_frame *);
bool checkva(const void* va);
bool decompose_dir(const char* dir, char* ret_name, struct dir** par_dir);
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
    struct thread* t = thread_current();

    /* Retrieve syscall number */
    uint32_t sys_no = read4(f, 0);
    
    void *buffer; 
    pid_t pid;
    
    /* The following is for page fault in syscall.
     * Set the thread's syscall status as true.
     * Store the current esp in the thread's esp field */
    t->syscall = true;
    t->esp = f->esp;

    /* Note after each syscall is about to finish, we will
     * set the thread's syscall status back to false. */
    switch (sys_no) {
        case SYS_HALT:
            halt();
            t->syscall = false;
            t->esp = NULL;
            break;
            
        case SYS_EXIT:
            status = (int) read4(f, 4);
            exit(status);
            t->syscall = false;
            t->esp = NULL;
            break;
            
        case SYS_EXEC:
            cmdline = (const char*) read4(f, 4);
            f->eax = (uint32_t) exec(cmdline);
            break;
            
        case SYS_WAIT:
            pid = (pid_t) read4(f, 4);
            f->eax = (uint32_t) wait(pid);
            t->syscall = false;
            t->esp = NULL;
            break;
            
        case SYS_CREATE:
            f_name = (const char*) read4(f, 4);
            f_size = (unsigned) read4(f, 8);
            f->eax = (uint32_t) create(f_name, f_size);
            t->syscall = false;
            t->esp = NULL;
            break;
            
        case SYS_REMOVE:
            f_name = (const char*) read4(f, 4);
            f->eax = (uint32_t) remove(f_name);
            t->syscall = false;
            t->esp = NULL;
            break;
            
        case SYS_OPEN:
            f_name = (const char*) read4(f, 4);
            f->eax = (uint32_t) open(f_name);
            break;
            
        case SYS_FILESIZE:
            fd = (uint32_t) read4(f, 4);
            f->eax = (uint32_t) filesize(fd);
            t->syscall = false;
            t->esp = NULL;
            break;
            
        case SYS_READ:
            fd = (uint32_t) read4(f, 4);
            buffer = (void*) read4(f, 8);
            size = (unsigned) read4(f, 12);
            f->eax = (uint32_t) read(fd, buffer, size);
            t->syscall = false;
            t->esp = NULL;
            break;
            
        case SYS_WRITE:
            fd = (uint32_t) read4(f, 4);
            buffer = (void*) read4(f, 8);
            size = (unsigned) read4(f, 12);
            f->eax = (uint32_t) write(fd, buffer, size);
            t->syscall = false;
            t->esp = NULL;
            break;
            
        case SYS_SEEK:
            fd = (uint32_t) read4(f, 4);
            position = (unsigned) read4(f, 8);
            seek(fd, position);
            t->syscall = false;
            t->esp = NULL;
            break;
            
        case SYS_TELL:
            fd = (uint32_t) read4(f, 4);
            f->eax = (uint32_t) tell(fd);
            t->syscall = false;
            t->esp = NULL;
            break;
            
        case SYS_CLOSE:
            fd = (uint32_t) read4(f, 4);
            close(fd);
            t->syscall = false;
            t->esp = NULL;
            break;

        case SYS_MMAP:
            fd = (uint32_t) read4(f, 4);
            buffer = (void*) read4(f, 8);
            f->eax = (uint32_t) mmap(fd, buffer);
            t->syscall = false;
            t->esp = NULL;
            break;

        case SYS_MUNMAP:
            mapping = (mapid_t) read4(f, 4);
            munmap(mapping);
            t->syscall = false;
            t->esp = NULL;
            break;
            
        case SYS_ISDIR:
            fd = (uint32_t) read4(f, 4);
            f->eax = (uint32_t) _isdir(fd);
            t->syscall = false;
            t->esp = NULL;
            break;
            
        case SYS_INUMBER:
            fd = (uint32_t) read4(f,4);
            f->eax = (uint32_t) _inumber(fd);
            t->syscall = false;
            t->esp = NULL;
            break;
            
        case SYS_CHDIR:
            f_name = (const char*) read4(f,4);
            f->eax = (uint32_t) _chdir(f_name);
            t->syscall = false;
            t->esp = NULL;
            break;
            
        case SYS_MKDIR:
            f_name = (const char*) read4(f,4);
            f->eax = (uint32_t) _mkdir(f_name);
            t->syscall = false;
            t->esp = NULL;
            break;
        
        case SYS_READDIR:
            fd = (uint32_t) read4(f,4);
            f_name = (char*) read4(f, 8);
            f->eax = (uint32_t) _readdir(fd, f_name);
            t->syscall = false;
            t->esp = NULL;
            break;

        default:
            exit(-1);
            t->syscall = false;
            t->esp = NULL;
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
    struct dir* cur_dir;
    char name[15];
    /* Checks the validity of the given pointer */
    if (!checkva(f_name) || !(*f_name))
        exit(-1);
    
    if (!decompose_dir(f_name, name, &cur_dir)){
        return false;
    }
    
    /* Create the file, while locking the file system. */
    lock_acquire(&filesys_lock);
    bool flag = filesys_dir_create(name, (off_t) initial_size, cur_dir);
    lock_release(&filesys_lock);


    return flag;

}

/*! Remove a file */
bool remove(const char *f_name) {
    struct dir* cur_dir;
    char name[15];
    /* Checks the validity of the given pointer */
    //printf("going to remove file %s\n\n", f_name);
    if (!checkva(f_name))
        exit(-1);
    
    if (strcmp(".", f_name) == 0 || strcmp("..", f_name) == 0)
        return false;

    if (!decompose_dir(f_name, name, &cur_dir))
        return false;

    /* Remove the file, while locking the file system. */
    lock_acquire(&filesys_lock);
    bool flag = filesys_dir_remove(name, cur_dir);
    lock_release(&filesys_lock);
    //printf("name decomposed! %x\n\n", cur_dir);
    
    dir_close(cur_dir);
    return flag;
}

/*! Open a file */
int open(const char *f_name) {
    struct dir* cur_dir;
    char name[15];
    struct thread *t;
    struct f_info *f;
    struct dir* d_open;
    struct file* f_open;
    bool isdir;
    uint32_t fd;
    struct inode* inode;

    /* Checks the validity of the given pointer */
    if (!checkva(f_name) || f_name[0] == '\0'){
       exit(-1);
    }
    
    if (!decompose_dir(f_name, name, &cur_dir)){
        return -1;
    }
    
    /* Open the file when locking the file system. */
    lock_acquire(&filesys_lock);
    
    if (strcmp(name, "\0") != 0){
        if (!dir_lookup(cur_dir, name, &inode)){
            lock_release(&filesys_lock);
            return -1;
        }
        isdir = (inode->data.type == DIR_INODE_DISK);
        //printf("setting isdir: %d", isdir);
        if (isdir)
            d_open = dir_open(inode);
        else
            f_open = file_open(inode);
        lock_release(&filesys_lock);
    } else {
        isdir = true;
        d_open = dir_reopen(cur_dir);
    }
    
    if (f_open == NULL && dir_open == NULL) {
        inode_close(inode);
        /* If file open failed, then exit with error. */
        return -1;
    } else {
        t = thread_current();
        if (t->f_count > 127){
            if (isdir){
                dir_close(d_open);
            } else {
                file_close(f_open);  
            }
            return -1;
        }
        
        /* Set up new f_info */
        f = (struct f_info*) malloc(sizeof(struct f_info));
        f->isdir = isdir;
        if (isdir)
            f->d = d_open;
        else
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
    uint8_t* addr_e;
    struct supp_table* st;
    
    /* Check the validity of given pointer */
    if ((!checkva(buffer)) || (!checkva(buffer + size))){
        exit(-1);
    }
    
    for (addr_e = (uint8_t*) pg_round_down(buffer); 
         addr_e < (uint8_t*) buffer + size; addr_e += PGSIZE){
        st = find_supp_table(addr_e);
        if (st && !st->writable)
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
        
        if (f->isdir)
            exit(-1);
        
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
    uint8_t* addr_e;
    struct supp_table *st;
    /* Check the validity of given pointer */
    if ((!checkva(buffer)) || (!checkva(buffer + size)))
        exit(-1);
    /* Checking we are not writing to unwritable pages. */
    
    int write_size = 0;
    
    if (fd == STDOUT_FILENO) {
        /* If std-out, then write using putbuf() */
        putbuf(buffer, size);
        write_size = size;
        
    } else {
        /* Otherwise, first find the file of this fd. */
        struct f_info* f = findfile(fd);
        if (f->isdir)
            exit(-1);
        
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
    
    if (f->isdir)
        dir_close(f->d);
    else
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

/*! Memory map from file to user address. */
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
    
    if (!checkva(addr))
        exit(-1);

    /* Check for the invalid conditions:
     * fd is standard io; file size of the given fd is 0; give user address
     * is not valie; given address is not page aligned; given address is 0.
     * If invalid, then return MAP_FAIL. */
    if (fd == STDIN_FILENO ||
        fd == STDOUT_FILENO ||
        filesize(fd) == 0 ||
        pg_ofs(addr) != 0 ||
        addr == 0) {
            return MAP_FAIL;
    }
    
    /* Get the file size of the file. And check the entire range of the
     * to-be-mapped user address does not overlap with any already allocated
     * pages. */
    f_size = filesize(fd);
    for (addr_e = addr; addr_e < addr + f_size; addr_e += PGSIZE){
            if (find_supp_table(addr_e) != NULL){
                /* If found a supplemental page entry of this page address,
                 * Then this is already alocated. Return MAP_FAIL. */
                return MAP_FAIL;
            }   
    }
    
    /* Increment the thread's max mmapid to give this mmapping a unque ID. */
    ++ t->mmapid_max;
    mapid = t->mmapid_max;
    
    /* Allocated the new mmap struct */
    me = (struct mmap_elem*) malloc(sizeof(struct mmap_elem));
    if (me == NULL)
        return MAP_FAIL;
    
    /* Reopen the file according to the file descriptor. */
    f = findfile(fd);
    if (f->isdir)
        return MAP_FAIL;
    lock_acquire(&filesys_lock);
    file = file_reopen(f->f);
    lock_release(&filesys_lock);
    
    /* If the file is NULL, then free the struct and return MAP_FAIL. */
    if (file == NULL){
        free(me);
        return MAP_FAIL;
    }
    
    /* Setup the fields of the mmap struct.*/
    me->file = file;
    me->mapid = mapid;
    /* Push the mmap struct to the list of mmap of this process. */
    list_push_back(&(t->mmap_lst), &(me->elem));
    list_init(&(me->s_table));
    
    /* Allocate pages for the read-in file data.*/
    upage = addr;
    ofs = 0;
    read_bytes = f_size;
    
    if (read_bytes >= PGSIZE)
        zero_bytes = 0;
    else
        zero_bytes = PGSIZE - read_bytes;
    
    while (read_bytes > 0) {
        /* Calculate how to fill this page.
           We will read PAGE_READ_BYTES bytes from FILE
           and zero the final PAGE_ZERO_BYTES bytes. */
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;
        
        /* Create a new supplemental page entry for this page*/
        st = create_mmap_supp_table(file, ofs, upage, page_read_bytes, 
                                    page_zero_bytes, true);
        /* Push the page entry to the mmap struct's list */
        list_push_back(&(me->s_table), &(st->map_elem));
        
        /* Update the remaining read_bytes, zero_bytes;
         * Update upage, and ofs. This is for the next page to load the file.*/
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        upage += PGSIZE;
        ofs += page_read_bytes;
        
    }
    return mapid;
}

/*! Memory ummap according the given mapid. */
void munmap(mapid_t mapping){
    
    uint32_t f_size, write_bytes, page_write_size, pws2;
    off_t ofs = 0;
    struct list_elem *e;
    struct supp_table* st;
    
    /* First find the mmap struct according to the given mapid. */
    struct mmap_elem *me = find_mmap_elem(mapping);
    struct thread* t = thread_current();
    
    /* Get the file length. And set write_bytes as file length.*/
    f_size = file_length(me->file);
    write_bytes = f_size;
    
    /* Freeing all the pages in this mmap struct. */
    while (!list_empty(&me->s_table)) {
        e = list_pop_front(&me->s_table);
        st = list_entry(e, struct supp_table, map_elem);
        if (st->fr) {
            /* Set up how many bytes is going to be freed in this page. */
            if (write_bytes >= PGSIZE)
                page_write_size = PGSIZE;
            else
                page_write_size = write_bytes;

            /* If the page is dirty, then write back the data to the file. */
            if (pagedir_is_dirty(t->pagedir, st->upage)){

                lock_acquire(&filesys_lock);
                pws2 = file_write_at(st->file, st->fr->physical_addr, 
                                                st->read_bytes, st->ofs);
                lock_release(&filesys_lock);
                ASSERT(pws2 == page_write_size);
            }
            
            /* Update the offset for file writing.*/
            ofs += page_write_size;
            /* Update remaining write_bytes.*/
            write_bytes -= page_write_size;
        }
        /* Destroy the freed supplemental page entry.*/
        spte_destructor_func(&(st->elem), NULL);
    }

    /* Close the file. */
    file_close(me->file);
    /* Remove the mmap struct from the mmap list of this process. */
    list_remove(&(me->elem));
    /* Free the memory of this struct. */
    free(me);
}

/*! Given a mapid, look for and return the corresponding mapid struct
 * of this process */
struct mmap_elem* find_mmap_elem(mapid_t mapid){
    
    struct list_elem *e;
    struct thread* t = thread_current();
    struct list* m_lst = &(t->mmap_lst);
    struct mmap_elem* me;
    
    /* Check for the validity of this mapid. */
    if (mapid == MAP_FAIL && mapid > t->mmapid_max) {
        exit(-1);
    }
    
    /* Iterate through the mapid list of the process, and check for
     * the mapid */
    for (e = list_begin(m_lst); e != list_end(m_lst); e = list_next(e)){
        me = list_entry(e, struct mmap_elem, elem);
        if (mapid == me->mapid)
            return me;
    }
    
    exit(-1);
}

int _inumber(uint32_t fd) {
    struct f_info* f = findfile(fd);
    if (f->isdir)
        return dir_get_inode(f->d)->sector;
    else
        return f->f->inode->sector;
}

bool _isdir(uint32_t fd){
    struct f_info* f = findfile(fd);

    return f->isdir;
}

bool _chdir(const char* dir){
    struct thread* t = thread_current();
    struct inode* next_inode;
    struct dir* cur_dir;
    char name[15];
    if (!checkva(dir)){
       exit(-1);
    }
   
    //printf("changing dir to %s\n\n", dir); 
    if (!decompose_dir(dir, name, &cur_dir))
        return false;
    if (strcmp(name, "\0") != 0){
        if (!dir_lookup(cur_dir, name, &next_inode)) {
            //printf("dir not found %s\n\n", name);
            dir_close(cur_dir);
            return false;
        }
        //printf("dir found\n\n");    
        dir_close(cur_dir);
            
        if  (next_inode->data.type != DIR_INODE_DISK) {
            inode_close(next_inode);
            return false;
        }
        //printf("is a dir\n\n");
        cur_dir = dir_open(next_inode);
        
        if (cur_dir == NULL) {
            inode_close(next_inode);
            return false;
        }
        //printf("open succeed\n\n");
        
    } else if (dir[strlen(dir)] != '/'){
        dir_close(cur_dir);
        return false;
    }
    
    dir_close(t->cur_dir);
    t->cur_dir = dir_reopen(cur_dir);
    dir_close(cur_dir);
    
    return true;
}

bool _mkdir(const char* dir) {
    struct inode* next_inode;
    struct dir* cur_dir;
    char name[15];
    block_sector_t sector;
    
    //printf("Checking name\n");
    if (!checkva(dir))
       exit(-1);
    //printf("done checking name\n");
    if (!decompose_dir(dir, name, &cur_dir)){
        return false;
    }
    ASSERT(cur_dir != NULL);
    if (!free_map_allocate(1, &sector) || 
        !dir_create(sector, 0, dir_get_inode(cur_dir)->sector)){
        
        dir_close(cur_dir);
        return false;
    }
    if (!dir_add(cur_dir, name, sector)){
        free_map_release(sector, 1);
        dir_close(cur_dir);
        return false;
    }
    
    dir_close(cur_dir);
    return true;
    
    
}


bool decompose_dir(const char* dir, char* ret_name, struct dir** par_dir){
    struct dir* cur_dir;
    struct inode* next_inode;
    unsigned i;
    struct thread* t = thread_current();
    char name[15];
    
    if (*dir == NULL || dir[0] == '\0')
        return false;

    if (dir[0] == '/') {
        cur_dir = dir_open_root();
    } else if (t->cur_dir == NULL) {
        cur_dir = dir_open_root();
        t->cur_dir = dir_open_root();
    } else {
        cur_dir = dir_reopen(t->cur_dir);    
    }

    while (*dir != '\0'){
        i = 0;
        while (*dir == '/')
            ++dir;
        while ((*(dir + i) != '/' && *(dir + i) != '\0') && (i <= 15)){
            ++i;
        }
        if (i > 15) {
            dir_close(cur_dir);
            return false;
        }

        memcpy(name, dir, i);

        name[i] = '\0';

        
        if (*(dir + i) == '\0')
            break;
        
        if (!dir_lookup(cur_dir, name, &next_inode)) {
            dir_close(cur_dir);
            return false;
        }

        dir_close(cur_dir);
        
        if  (next_inode->data.type != DIR_INODE_DISK) {
            inode_close(next_inode);
            return false;
        }
        
        cur_dir = dir_open(next_inode);
        
        if (cur_dir == NULL) {
            inode_close(next_inode);
            return false;
        }
        
        dir += i;
    }
    *par_dir = cur_dir;

    strlcpy(ret_name, name, strlen(name) + 1);

    return true;
}

bool _readdir(uint32_t fd, char* name){
    if (!checkva(name))
        return false;
    
    struct f_info* f = findfile(fd);
    
    if (!f->isdir)
        return false;
    return dir_readdir(f->d, name);
}

