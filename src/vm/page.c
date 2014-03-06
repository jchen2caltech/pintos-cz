#include "vm/page.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <stdio.h>
#include <stdlib.h>
#include <list.h>
#include <string.h>

unsigned spte_hash_func(struct hash_elem *h, void *aux UNUSED);
bool spte_less_func(struct hash_elem *h1, struct hash_elem *h2, 
                     void *aux UNUSED);

void supp_table_init(struct hash* s_table) {
    hash_init(s_table, spte_hash_func, spte_less_func, NULL);
}


struct supp_table * find_supp_table(void *virtual_addr){
    struct supp_table st;
    struct hash_elem *e;
    struct thread* t = thread_current();
    /*printf("Looking for supp_page %x\n", virtual_addr);*/
    
    st.upage = pg_round_down(virtual_addr);
    e = hash_find(&t->s_table, &st.elem);
    
    return e != NULL ? hash_entry(e, struct supp_table, elem) : NULL;
}

struct supp_table * create_supp_table(struct file *file, off_t ofs, 
                                      uint8_t *upage, uint32_t read_bytes,
                                      uint32_t zero_bytes, bool writable) {
    ASSERT(read_bytes + zero_bytes == PGSIZE);
    /*printf("Creating supp_page %x in thread: %s\n", upage, thread_current()->name);*/
    struct supp_table* st;
    st =(struct supp_table*) malloc(sizeof(struct supp_table));
        
    if (st == NULL) {
        /*printf("Cannot allocate sup_table.\n");*/
        exit(-1);
    }
        
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
    hash_insert(&(thread_current()->s_table), &st->elem);
    
    return st;
}

struct supp_table * create_stack_supp_table(void* virtual_addr){
    struct supp_table* st;
    st =(struct supp_table*) malloc(sizeof(struct supp_table));
        
    if (st == NULL) {
        /*printf("Cannot allocate sup_table.\n");*/
        exit(-1);
    }
    st->type = SPT_STACK;
    st->writable = true;
    st->upage = virtual_addr;
    st->fr = NULL;
    st->pinned = true;
    if (intr_context())
        st->pinned = false;
    hash_insert(&(thread_current()->s_table), &st->elem);
    return st;
}

struct supp_table * create_mmap_supp_table(void *virtual_addr){
    ASSERT(read_bytes + zero_bytes == PGSIZE);
    /*printf("Creating supp_page %x in thread: %s\n", upage, thread_current()->name);*/
    struct supp_table* st;
    st =(struct supp_table*) malloc(sizeof(struct supp_table));
        
    if (st == NULL) {
        /*printf("Cannot allocate sup_table.\n");*/
        exit(-1);
    }
        
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
    hash_insert(&(thread_current()->s_table), &st->elem);
    
    return st;
    
}

unsigned spte_hash_func(struct hash_elem *h, void *aux UNUSED) {
    struct supp_table * st = hash_entry(h, struct supp_table, elem);
    return hash_bytes(&st->upage, sizeof st->upage);
}
bool spte_less_func(struct hash_elem *h1, struct hash_elem *h2, 
                     void *aux UNUSED){
    struct supp_table *s1 = hash_entry(h1, struct supp_table, elem);
    struct supp_table *s2 = hash_entry(h2, struct supp_table, elem);
    return (s1->upage < s2->upage);
}
