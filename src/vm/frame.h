#ifndef FRAME_H
#define FRAME_H
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "vm/page.h"
#include <list.h>
#include "threads/synch.h"
#include "threads/palloc.h"

struct frame_table {
    struct list table;
    struct lock lock;
};

struct frame_table_entry {
    void *physical_addr;
    struct supp_table *spt;
    struct list_elem elem;
    struct thread *owner;
};

void frame_table_init(void);

struct frame_table_entry *obtain_frame(enum palloc_flags flag, 
                                       struct supp_table *pte);

struct frame_table f_table;
#endif