#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <stdio.h>
#include <stdlib.h>
#include <list.h>
#include <string.h>
#include <bitmap.h>

static struct frame_table f_table;

struct frame_table_entry *evict_frame(void);

void frame_table_init(int num_frames) {
    list_init(&f_table.table);
    lock_init(&(&f_table)->lock);
    (&f_table)->frames = bitmap_create((size_t)num_frames);
}


struct frame_table_entry *obtain_frame(enum palloc_flags flag, 
                                       struct supp_table *pte) {
    void *page;
    struct frame_table_entry *newframe;
    size_t idx;

    idx = bitmap_scan_and_flip((&f_table)->frames, 0, 1, false);
    if (idx == BITMAP_ERROR)
        PANIC("Run out of frames\n");
    else {
        page = palloc_get_page(flag);
        if (page) {
            newframe = (struct frame_table_entry *)\
                       malloc(sizeof(struct frame_table_entry));
            if (!newframe)
                PANIC("malloc failure\n");
            newframe->physical_addr = page;
            newframe->owner = thread_current();
            newframe->spt = pte;
        }
        else {
            PANIC("Run out of frames\n");
        }
        list_push_back(&f_table.table, &newframe->elem);
    }
    return newframe;
}

void * frame_evict(void *virtual_addr) {

    lock_acquire(&f_table.lock);


    lock_release(&f_table.lock);
}