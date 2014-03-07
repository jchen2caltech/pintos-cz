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
#include "devices/block.h"
#include "threads/synch.h"

/*! Initialize the swap slot. This includes allocating the swap_block, 
    setting up the bitmap resouce manager, and initializing the swap_lock. */

void swap_init(void) {
    swap_block = block_get_role(BLOCK_SWAP);
    if (swap_block) {
        swap_bm = bitmap_create(block_size(swap_block) / SECTORS_PER_PAGE);
        if (swap_bm)
            bitmap_set_all(swap_bm, 0);
    }
    lock_init(&swap_lock);
}

/*! Swap out a frame. */
size_t swap_out(void *frame) {
    size_t position, i;

    if (!swap_block || !swap_bm)    /* Swap block non-existent */
        PANIC("swapping partition not present!\n", 
              (int)swap_block, (int) swap_bm);
    if (swap_lock.holder != thread_current())
        lock_acquire(&swap_lock);

    /* Mark the swap slot as used. */
    position = bitmap_scan_and_flip(swap_bm, 0, 1, false);
    if (position == BITMAP_ERROR)
        PANIC("no free swapping partition available!\n");
    for (i = 0; i < SECTORS_PER_PAGE; i++) {
        /* Write the content of the frame into swap-block */
        block_write(swap_block, position * SECTORS_PER_PAGE + i,
                    (uint8_t *)frame + i * BLOCK_SECTOR_SIZE);
    }
    lock_release(&swap_lock);
    return position;
}

/*! Swap in a frame. */
void swap_in(void *frame, size_t position) {
    size_t i;

    if (!swap_block || !swap_bm)    /* Swap block non-existent */
        PANIC("swapping partition not present!\n");
    if (swap_lock.holder != thread_current())
        lock_acquire(&swap_lock);
    if (!bitmap_test(swap_bm, position)) /* Not a used swap slot */
        PANIC("attempt to swap in a free frame! %d\n", position);

    /* Mark the slot as free */
    bitmap_flip(swap_bm, position);
    for (i = 0; i < SECTORS_PER_PAGE; i++) {
        /* Read the content of the swap slot back into the frame */
        block_read(swap_block, position * SECTORS_PER_PAGE + i,
                   (uint8_t *)frame + i * BLOCK_SECTOR_SIZE);
    }
    lock_release(&swap_lock);
}
