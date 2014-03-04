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
void supp_table_init(void);

struct frame_table_entry *obtain_frame(void *virtual_addr);
struct supp_table * find_supp_table(void *virtual_addr)

struct supp_table {
   struct file* file;
   off_t ofs;
   uint8_t * upage;
   uint32_t read_bytes;
   uint32_t zero_bytes;
   bool writable;
   struct swap_table* swap_slot;
   struct frame_table_entry* fr;
   struct list_elem elem;
   
   
}

#endif