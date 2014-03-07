#ifndef FRAME_H
#define FRAME_H
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "vm/page.h"
#include <list.h>
#include "threads/synch.h"
#include "threads/palloc.h"

struct frame_table {
    struct list table;          /*! Contains all allocated frames */
    struct lock lock;           /*! Used for synchronization */
    uint32_t hand;              /*! Current next element to be checked */
};

struct frame_table_entry {
    void *physical_addr;        /*! Address of the actual frame */
    struct supp_table *spt;     /*! Supp pagetable entry for the page */
    struct list_elem elem;      /*! List element */
    struct thread *owner;       /*! Thread that owns the page */
};

void frame_table_init(void);

struct frame_table_entry *obtain_frame(enum palloc_flags flag, 
                                       struct supp_table *pte);

struct frame_table f_table;
#endif