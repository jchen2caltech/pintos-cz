#ifndef FRAME_H
#define FRAME_H
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "vm/page.h"

struct frame_table {
    struct hash table;
    struct lock lock;
    struct bitmap *frames;
};

struct frame_table_entry {
    void *physical_addr;
    void *virtual_addr;
    struct supp_table *spt;
    struct hash_elem elem;
    struct thread *owner;
};

void frame_table_init(int num_frames);

struct frame_table_entry *obtain_frame(void *virtual_addr);


#endif