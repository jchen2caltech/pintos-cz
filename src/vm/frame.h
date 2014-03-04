#ifndef FRAME_H
#define FRAME_H
#include "threads/thread.h"
#include "vm/page.h"

struct frame_table_entry {
    void *physical_addr;
    void *virtual_addr;
    struct supp_table *spt;
    struct list_elem elem;
    struct thread *owner;
};

void frame_table_init(void);
struct frame_table_entry *obtain_frame(void *virtual_addr);

#endif