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

void frame_table_init(void) {
    list_init(&f_table.table);
    lock_init(&f_table.lock);
}


struct frame_table_entry *obtain_frame(enum palloc_flags flag, 
                                       struct supp_table *pte) {
    void *page;
    struct frame_table_entry *newframe;

    page = palloc_get_page(flag);
    if (!page)
        page = frame_evict(flag);
    if (!page) 
        PANIC("run out of frames!\n");

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

/* clock algorithm: circularly loop through every frame */
void * frame_evict(enum palloc_flags flag) {
    struct list_elem *ce;
    struct thread *ct;
    struct frame_table_entry *cf;
    bool one_round = false;
    
    if (f_table.lock.holder != thread_current())
        lock_acquire(&f_table.lock);
    if (list_empty(&f_table.table)) {
        lock_release(&f_table.lock);
        return NULL;
    }
    ce = list_begin(&f_table.table);
    while (true) {
        cf = list_entry(ce, struct frame_table_entry, elem);
        ct = cf->owner;
        if (!cf->spt->pinned) {
            if (pagedir_is_accessed(ct->pagedir, cf->spt->upage))
                pagedir_set_accessed(ct->pagedir, cf->spt->upage, false);
            else {
                if (cf->spt->type != SPT_MMAP) {
                    cf->spt->type = SPT_SWAP;
                    cf->spt->swap_index = swap_out(cf->physical_addr);
                }
                else if (cf->spt->type == SPT_MMAP && 
                         pagedir_is_dirty(ct->pagedir, cf->spt->upage)) {
                    file_write_at(cf->spt->file, cf->physical_addr, 
                                  cf->spt->read_bytes, cf->spt->ofs);
                }
                list_remove(ce);
                pagedir_clear_page(ct->pagedir, cf->spt->upage);
                palloc_free_page(cf->physical_addr);
                cf->spt->fr = NULL;
                free(cf);
                lock_release(&f_table.lock);
                return palloc_get_page(flag);
            }
        }
        ce = list_next(ce);
        if (ce == list_end(&f_table.table)) {
            if (!one_round) {
                ce = list_begin(&f_table.table);
                /*one_round = true;*/
            } else {
                lock_release(&f_table.lock);
                return NULL;
            }
        }
    }
}
