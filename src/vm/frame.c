#include "vm/frame.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <stdio.h>
#include <stdlib.h>
#include <list.h>
#include <string.h>

static struct list frame_table;
static struct list supp_table_lst;

struct frame_table_entry *evict_frame(void);

void frame_table_init(void) {
    list_init(&frame_table);
}

void supp_table_init(void) {
    list_init(&supp_table_lst);
}

struct frame_table_entry *obtain_frame(void *virtual_addr) {
    void *page;
    struct frame_table_entry *newframe;

    page = palloc_get_page(PAL_USER);
    if (page) {
        newframe = (struct frame_table_entry *)malloc(sizeof(struct frame_table_entry));
        newframe->physical_addr = page;
        newframe->owner = thread_current();
        newframe->virtual_addr = virtual_addr;
    }
    else {
        fprintf(stderr, "Run out of frames!\n");
        exit(-1);
    }
    list_push_back(&frame_table, &newframe->elem);
    return newframe;
}

struct supp_table * find_supp_table(void *virtual_addr){
    struct list_elem *e;
    for (e = list_begin(&supp_table_lst); e != list_end(&supp_table_lst); 
        e = list_next(e)){
        struct supp_table* st = list_entry(e, struct supp_table, elem);
        if (pg_no(virtual_addr) == pg_no(st->upage))
            return st;
    }
    return NULL;
}