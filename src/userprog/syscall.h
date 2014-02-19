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
#include "filesys/file.h"
#include "devices/input.h"
#include "userprog/pagedir.h"

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


#endif /* userprog/syscall.h */

