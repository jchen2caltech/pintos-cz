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

void supp_table_init(struct hash* s_table) {
    hash_init(s_table, spte_hash_func, spte_less_func, NULL);
}


struct supp_table * find_supp_table(void *virtual_addr){
    struct supp_table st;
    struct hash_elem *e;
    struct thread* t = thread_current();
    /*printf("Looking for supp_page %x\n", (int)virtual_addr);
    */
    st.upage = pg_round_down(virtual_addr);
    /*printf("Find %x hash size %d\n", st.upage, hash_size(&t->s_table));
    */
    e = hash_find(&t->s_table, &st.elem);
    /*printf("Find2\n");*/
    return e != NULL ? hash_entry(e, struct supp_table, elem) : NULL;
}

struct supp_table * create_supp_table(struct file *file, off_t ofs, 
                                      uint8_t *upage, uint32_t read_bytes,
                                      uint32_t zero_bytes, bool writable) {
    ASSERT(read_bytes + zero_bytes == PGSIZE);
    
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
    /*printf("hash size before creating: %d\n", hash_size(&(thread_current()->s_table)));
    */
    hash_insert(&(thread_current()->s_table), &st->elem);
    /*printf("hash size after creating: %d\n", hash_size(&(thread_current()->s_table)));
    */
   /*printf("Creating file supp_page %x in thread: %s %x\n", upage, thread_current()->name, (int)st);*/
   /*
   if (writable)
       printf("The page is writable.\n");
   else
       printf("it is not wriable.\n");*/

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
    /*printf("Creating stack supp_page %x in thread: %s\n", virtual_addr, thread_current()->name);
    */
    return st;
}

struct supp_table * create_mmap_supp_table(struct file *file, off_t ofs, 
                                      void *upage, uint32_t read_bytes,
                                      uint32_t zero_bytes, bool writable){
    ASSERT(read_bytes + zero_bytes == PGSIZE);
    /*printf("Creating supp_page %x in thread: %s\n", upage, thread_current()->name);
    */
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
    /*printf("Creating mmap supp_page %x in thread: %s\n", upage, thread_current()->name);
    */
    
    return st;
    
}

unsigned spte_hash_func(struct hash_elem *h, void *aux UNUSED) {
    struct supp_table * st = hash_entry(h, struct supp_table, elem);
    return hash_int((int)st->upage);
}
bool spte_less_func(struct hash_elem *h1, struct hash_elem *h2, 
                     void *aux UNUSED){
    struct supp_table *s1 = hash_entry(h1, struct supp_table, elem);
    struct supp_table *s2 = hash_entry(h2, struct supp_table, elem);
    return (s1->upage < s2->upage);
}

void spte_destructor_func(struct hash_elem *h, void *aux UNUSED) {
    struct supp_table *s = hash_entry(h, struct supp_table, elem);
    hash_delete(&thread_current()->s_table, h);

    if (s->fr) {
        lock_acquire(&f_table.lock);
        list_remove(&s->fr->elem);
        pagedir_clear_page(s->fr->owner->pagedir, s->upage);
        palloc_free_page(s->fr->physical_addr);
        free(s->fr);
        lock_release(&f_table.lock);
    }
    else if (s->type == SPT_SWAP) {
        lock_acquire(&swap_lock);
        bitmap_set(swap_bm, s->swap_index, 0);
        lock_release(&swap_lock);
    }
    free(s);
}
