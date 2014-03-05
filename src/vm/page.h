#ifndef PAGE_H
#define PAGE_H
#include "threads/thread.h"
#include "userprog/syscall.h"
#include <hash.h>
#include "vm/frame.h"
#define SPT_FILE 0
#define SPT_SWAP 1
#define SPT_NULL 2
#define SPT_STACK 3

struct supp_table {
   struct file* file;
   off_t ofs;
   int type;
   size_t swap_index;
   uint8_t * upage;
   uint32_t read_bytes;
   uint32_t zero_bytes;
   bool writable;
   struct swap_table* swap_slot;
   struct frame_table_entry* fr;
   struct hash_elem elem; 
};

void supp_table_init(struct hash* s_table);
struct supp_table * find_supp_table(void *virtual_addr);
struct supp_table * create_supp_table(struct file *file, off_t ofs, 
                                      uint8_t *upage, uint32_t read_bytes,
                                      uint32_t zero_bytes, bool writable);
struct supp_table * create_stack_supp_table(void *virtual_addr);
#endif 
