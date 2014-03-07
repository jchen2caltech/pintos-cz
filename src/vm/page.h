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
#define SPT_MMAP 4

struct supp_table {
   struct file* file;           /*! File to be loaded (if any) */
   off_t ofs;                   /*! Offset in file to be loaded (if any) */
   int type;                    /*! Type of supp-table 
                                    (FILE, MMAP, STACK or SWAP) */
   size_t swap_index;           /*! Position in swap-slot (if in) */
   uint8_t * upage;             /*! Virtual address of the page */
   uint32_t read_bytes;         /*! Num of bytes to read */
   uint32_t zero_bytes;         /*! Num of bytes to be set to 0 */
   bool writable;               /*! If the page is writable */
   bool pinned;                 /*! If the page is pinned (non-evictable) */
   struct frame_table_entry* fr;    /*! Frame the page is using */
   struct hash_elem elem;       /*! Hash element (for supp-table) */
   struct list_elem map_elem;   /*! List element (for mmap-list) */
};


void supp_table_init(struct hash* s_table);
struct supp_table * find_supp_table(void *virtual_addr);
struct supp_table * create_supp_table(struct file *file, off_t ofs, 
                                      uint8_t *upage, uint32_t read_bytes,
                                      uint32_t zero_bytes, bool writable);
struct supp_table * create_stack_supp_table(void *virtual_addr);

struct supp_table * create_mmap_supp_table(struct file *file, off_t ofs, 
                                      void *upage, uint32_t read_bytes,
                                      uint32_t zero_bytes, bool writable);

void spte_destructor_func(struct hash_elem *h, void *aux UNUSED);

#endif 
