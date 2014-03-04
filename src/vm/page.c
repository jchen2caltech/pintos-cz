#include "vm/page.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <stdio.h>
#include <stdlib.h>
#include <list.h>
#include <string.h>

static struct list supp_table_lst;

void supp_table_init(void) {
    list_init(&supp_table_lst);
}


struct supp_table * find_supp_table(void *virtual_addr){
    /*printf("Looking for supp_page %x\n", virtual_addr);*/
    struct list_elem *e;
    for (e = list_begin(&supp_table_lst); e != list_end(&supp_table_lst); 
        e = list_next(e)){
        struct supp_table* st = list_entry(e, struct supp_table, elem);
        if (pg_no(virtual_addr) == pg_no(st->upage))
            return st;
    }
    return NULL;
}

struct supp_table * create_supp_table(struct file *file, off_t ofs, 
                                      uint8_t *upage, uint32_t read_bytes,
                                      uint32_t zero_bytes, bool writable) {
    ASSERT(read_bytes + zero_bytes == PGSIZE);
    /*printf("Creating supp_page %x\n", upage);*/
    struct supp_table* st;
    st =(struct supp_table*) malloc(sizeof(struct supp_table));
        
    if (st == NULL) {
        printf("Cannot allocate sup_table.\n");
        exit(-1);
    }
        
    st->file = file;
    st->ofs = ofs;
    st->upage = upage;
    st->read_bytes = read_bytes;
    st->zero_bytes = zero_bytes;
    st->writable = writable;
    st->swap_slot = NULL;
    st->fr = NULL;
    list_push_back(&supp_table_lst, &st->elem);
    
    return st;
}