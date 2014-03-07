#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <list.h>
#include <string.h>


void * frame_evict(enum palloc_flags flag);

/*! Initialize the frame table global variable */
void frame_table_init(void) {
    list_init(&f_table.table);
    lock_init(&f_table.lock);
    f_table.hand = 0;
}

/*! Obtain a frame, either through palloc() or through eviction */
struct frame_table_entry *obtain_frame(enum palloc_flags flag, 
                                       struct supp_table *pte) {
    void *page;
    struct frame_table_entry *newframe;

    page = palloc_get_page(flag);
    if (!page)
        page = frame_evict(flag);
    if (!page) 
        PANIC("run out of frames!\n");

    /*! Allocate a frame table entry and initialize it */
    newframe = (struct frame_table_entry *)\
               malloc(sizeof(struct frame_table_entry));
    if (!newframe)
        PANIC("malloc failure\n");
    newframe->physical_addr = page;
    newframe->owner = thread_current();
    newframe->spt = pte;
    if (f_table.lock.holder != thread_current())
        lock_acquire(&f_table.lock);
    list_push_back(&f_table.table, &newframe->elem);
    lock_release(&f_table.lock);
    return newframe;
}

/*! Use the clock algorithm to select a frame to evict and execute the
    eviction. */
void * frame_evict(enum palloc_flags flag) {
    struct list_elem *ce;
    struct thread *ct;
    struct frame_table_entry *cf;
    int i;
    
    if (f_table.lock.holder != thread_current())
        lock_acquire(&f_table.lock);    /* Get frame table lock */
    if (list_empty(&f_table.table)) {
        lock_release(&f_table.lock);    /* Nothing to evict */
        return NULL;
    }
    f_table.hand = f_table.hand % list_size(&f_table.table);
    ce = list_begin(&f_table.table);    /* Begin iteration */
    for (i = 0; i < f_table.hand; i++) {
        /* Find where the "hand" points to: that is where the 
           iteration starts */
        ce = list_next(ce);
    }
    while (true) {
        cf = list_entry(ce, struct frame_table_entry, elem);
        ct = cf->owner;
        if (!cf->spt->pinned) {
            /* If pinned, skip this frame */
            if (pagedir_is_accessed(ct->pagedir, cf->spt->upage))
                /* If accessed, clear accessed bit */
                pagedir_set_accessed(ct->pagedir, cf->spt->upage, false);
            else {
                /* Swap out the page! */
                if (cf->spt->type != SPT_MMAP) {
                    cf->spt->type = SPT_SWAP;
                    cf->spt->swap_index = swap_out(cf->physical_addr);
                }
                /* Write the frame back to the map'd file */
                else if (cf->spt->type == SPT_MMAP &&
                         pagedir_is_dirty(ct->pagedir, cf->spt->upage)) {
                    cf->spt->fr = NULL;
                    file_write_at(cf->spt->file, cf->physical_addr, 
                                  cf->spt->read_bytes, cf->spt->ofs);
                }
                /* Clear the frame */
                list_remove(ce);
                pagedir_clear_page(ct->pagedir, cf->spt->upage);
                palloc_free_page(cf->physical_addr);
                free(cf);
                lock_release(&f_table.lock);
                return palloc_get_page(flag);
            }
        }
        ce = list_next(ce);
        ++f_table.hand;
        if (ce == list_end(&f_table.table)) { /* Iterate circularly */
            ce = list_begin(&f_table.table);
            f_table.hand = 0;
        }
    }
}
