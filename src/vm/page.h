#ifndef PAGE_H
#define PAGE_H
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "vm/frame.h"

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
};

void supp_table_init(void);
struct supp_table * find_supp_table(void *virtual_addr);
struct supp_table * create_supp_table(struct file *file, off_t ofs, 
                                      uint8_t *upage, uint32_t read_bytes,
                                      uint32_t zero_bytes, bool writable);
#endif 
