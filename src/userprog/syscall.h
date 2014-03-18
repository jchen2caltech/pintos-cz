#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "filesys/inode.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "userprog/pagedir.h"

typedef int mapid_t;
#define MAP_FAIL ((mapid_t) -1)

struct mmap_elem {
    mapid_t mapid;              /*! map id */
    struct list s_table;        /*! supp table of pages associated with 
                                    this map */
    struct file* file;          /*! file pointer */
    struct list_elem elem;      /*! List element to enlist in thread */
};

void syscall_init(void);
void halt(void);
void exit(int status);
pid_t exec(const char *cmd_line);
int wait(pid_t pid);
bool create(const char *f_name, unsigned initial_size);
bool remove(const char *f_name);
int open(const char *file);
int filesize(uint32_t fd);
int read(uint32_t fd, void *buffer, unsigned size);
int write(uint32_t fd, const void *buffer, unsigned size);
void seek(uint32_t fd, unsigned position);
unsigned tell(uint32_t fd);
void close(uint32_t fd);
mapid_t mmap(uint32_t fd, void* addr);
void munmap(mapid_t mapping);

struct mmap_elem* find_mmap_elem(mapid_t mapid);

bool _chdir(const char* dir);
bool _mkdir(const char* dir);
bool _readdir(uint32_t fd, char* name);
bool _isdir(uint32_t fd);
int _inumber(uint32_t fd);


#endif /* userprog/syscall.h */

