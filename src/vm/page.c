#include "vm/page.h"
#include "vm/swap.h"
#include "vm/frame.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <stdio.h>
#include <stdlib.h>
#include <list.h>
#include <string.h>
#include <bitmap.h>

unsigned spte_hash_func(struct hash_elem *h, void *aux UNUSED);
bool spte_less_func(struct hash_elem *h1, struct hash_elem *h2, 
                     void *aux UNUSED);

/*! Set up the hash table for the supplemental page entries
   of a given thread's s_table. */
void supp_table_init(struct hash* s_table) {
    hash_init(s_table, spte_hash_func, spte_less_func, NULL);
}

/*! Given a virtual address, look for the corresponding
    supplemental page entry in the thread. Return
    NULL if not found. */
struct supp_table * find_supp_table(void *virtual_addr){
    struct supp_table st;
    struct hash_elem *e;
    struct thread* t = thread_current();
    
    /* Round down the virtual address to a page */
    st.upage = pg_round_down(virtual_addr);
    
    /* Look for the page by using hash_find */
    e = hash_find(&t->s_table, &st.elem);

    return e != NULL ? hash_entry(e, struct supp_table, elem) : NULL;
}

/*! Creating a supplemental page entry, used in load segment */
struct supp_table * create_supp_table(struct file *file, off_t ofs, 
                                      uint8_t *upage, uint32_t read_bytes,
                                      uint32_t zero_bytes, bool writable) {
    ASSERT(read_bytes + zero_bytes == PGSIZE);
    
    
    /* Allocate the new supplemental page entry */
    struct supp_table* st;
    st =(struct supp_table*) malloc(sizeof(struct supp_table));
        
    if (st == NULL) {
        exit(-1);
    }
    
    /* Initialize each field according to the input */
    st->type = SPT_FILE;
    st->swap_index = 0;
    st->file = file;
    st->ofs = ofs;
    st->upage = upage;
    st->read_bytes = read_bytes;
    st->zero_bytes = zero_bytes;
    st->writable = writable;
    st->swap_slot = NULL;
    st->fr = NULL;
    st->pinned = false;
   
    /* Insert the new entry to the s_table of the process */
    hash_insert(&(thread_current()->s_table), &st->elem);
   
    return st;
}

/*! Creating a new stack supplemental page entry */
struct supp_table * create_stack_supp_table(void* virtual_addr){
    struct supp_table* st;
    
    /* Allocate the new supplemental page entry */
    st =(struct supp_table*) malloc(sizeof(struct supp_table));
    if (st == NULL) {
        exit(-1);
    }
    
    /* Initialize the fields of the entry */
    st->type = SPT_STACK;
    st->writable = true;
    st->upage = virtual_addr;
    st->fr = NULL;
    st->pinned = true;
    if (intr_context())
        st->pinned = false;
    
    /* Insert the new entry to the s_table of the process */
    hash_insert(&(thread_current()->s_table), &st->elem);
    
    return st;
}

/*! Creating a new memory-map supplemental page entry */
struct supp_table * create_mmap_supp_table(struct file *file, off_t ofs, 
                                      void *upage, uint32_t read_bytes,
                                      uint32_t zero_bytes, bool writable){
    ASSERT(read_bytes + zero_bytes == PGSIZE);
    struct supp_table* st;
    
    /* Allocate the new supplemental page entry */
    st =(struct supp_table*) malloc(sizeof(struct supp_table));
    if (st == NULL) {
        exit(-1);
    }
        
    /* Initialize the fields of the entry */
    st->type = SPT_MMAP;
    st->swap_index = 0;
    st->file = file;
    st->ofs = ofs;
    st->upage = upage;
    st->read_bytes = read_bytes;
    st->zero_bytes = zero_bytes;
    st->writable = writable;
    st->swap_slot = NULL;
    st->fr = NULL;
    st->pinned = false;
    
    /* Insert the new entry to the s_table of the process */
    hash_insert(&(thread_current()->s_table), &st->elem);
    
    return st;
    
}

/*! The hash function for the supplemental page entry's 
    hash table (s_table) for each process. We use the virtual address
    as the hash key, i.e. upage. */
unsigned spte_hash_func(struct hash_elem *h, void *aux UNUSED) {
    struct supp_table * st = hash_entry(h, struct supp_table, elem);
    return hash_int((int)st->upage);
}

/*! hash less function for the supplemental page entry's hash table.
    We simply compare the upage of two page entries. */
bool spte_less_func(struct hash_elem *h1, struct hash_elem *h2, 
                     void *aux UNUSED){
    struct supp_table *s1 = hash_entry(h1, struct supp_table, elem);
    struct supp_table *s2 = hash_entry(h2, struct supp_table, elem);
    return (s1->upage < s2->upage);
}

/*! Desctructor for the supplemental page entry. */
void spte_destructor_func(struct hash_elem *h, void *aux UNUSED) {
    struct supp_table *s = hash_entry(h, struct supp_table, elem);
    hash_delete(&thread_current()->s_table, h);

    if (s->fr) {
        /* If the frame is not swapped */
        lock_acquire(&f_table.lock);
        list_remove(&s->fr->elem);
        pagedir_clear_page(s->fr->owner->pagedir, s->upage);
        palloc_free_page(s->fr->physical_addr);
        free(s->fr);
        lock_release(&f_table.lock);
    }
    else if (s->type == SPT_SWAP) {
        /* If it is in swap partitions */
        lock_acquire(&swap_lock);
        bitmap_set(swap_bm, s->swap_index, 0);
        lock_release(&swap_lock);
    }
    free(s);
}
