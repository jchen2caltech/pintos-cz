#include "vm/frame.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include <stdio.h>
#include <stdlib.h>
#include <list.h>
#include <string.h>
#include <hash.h>
#include <bitmap.h>

static struct frame_table f_table;

struct frame_table_entry *evict_frame(void);

void frame_table_init(int num_frames) {
    hash_init(&(&f_table)->table, frame_hash_func, frame_less_func, NULL);
    lock_init(&(&f_table)->lock);
    (&f_table)->frames = bitmap_create((size_t)num_frames);
}


struct frame_table_entry *obtain_frame(void *virtual_addr) {
    void *page;
    struct frame_table_entry *newframe;

    page = palloc_get_page(PAL_USER);
    if (page) {
        newframe = (struct frame_table_entry *)malloc(sizeof(struct frame_table_entry));
        if (!newframe)
            PANIC("malloc failure\n");
        newframe->physical_addr = page;
        newframe->owner = thread_current();
        newframe->virtual_addr = virtual_addr;
    }
    else {
        PANIC("Run out of frames\n");
    }
    hash_insert(&(&f_table)->table, &newframe->elem);
    return newframe;
}

void frame_hash_func(struct hash_elem *h, void *aux UNUSED) {
    struct frame_table_entry *fte = hash_entry(h, struct frame_table_entry, 
                                               elem);
    return hash_bytes(&fte->physical_addr, sizeof fte->physical_addr);
}

void frame_less_func(struct hash_elem *h1, struct hash_elem *h2, 
                     void *aux UNUSED) {
    struct frame_table_entry *f1 = hash_entry(h1, struct frame_table_entry,
                                              elem);
    struct frame_table_entry *f2 = hash_entry(h2, struct frame_table_entry,
                                              elem);
    return (f1->physical_addr < f2->physical_addr);
}