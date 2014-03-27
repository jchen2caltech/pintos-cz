#include <list.h>
#include "devices/block.h"
#include "devices/timer.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/cache.h"
#include "threads/thread.h"
#include "threads/malloc.h"

block_sector_t just_read;
struct semaphore read_ahead_lock;

/*! Initialize the cache system */
void cache_init(void) {
    list_init(&filesys_cache.cache_list);
    lock_init(&filesys_cache.cache_lock);
    filesys_cache.cache_count = 0;
    filesys_cache.evict_pointer = NULL;
    sema_init(&read_ahead_lock, 0);
    /* Create ghost thread for periodical write-back */
    thread_create("cache write background", PRI_DEFAULT, 
                  cache_write_background, NULL);
    thread_create("cache read ahead", PRI_MIN + 1, cache_read_ahead, NULL);
}

/*! Find a cache in the cache list that corresponds to a given sector */
struct cache_entry *cache_find(block_sector_t sector) {
    struct list_elem *curr = list_begin(&filesys_cache.cache_list);
    struct cache_entry *curr_cache;

    while (curr && curr->next) {
        curr_cache = list_entry(curr, struct cache_entry, elem);
        if (curr_cache->sector == sector) {
            curr_cache->accessed = true;
            return curr_cache;
        }
        curr = list_next(curr);
    }
    return NULL;
}

/*! Find a cache that corresponds to a given sector, or create one if needed,
    and import the sector from the disk if the cache is created here */
static struct cache_entry *cache_readin(block_sector_t sector, bool dirty) {

    struct cache_entry *result;
    
    /* If the cache already exists */
    if ((result = cache_find(sector)) != NULL) {
        result->dirty |= dirty;
        result->open_count++;
        result->accessed = true;
        return result;
    }
    /* If there is room for one more cache block, create one */
    if (filesys_cache.cache_count < CACHE_MAXSIZE) {
        result = malloc(sizeof(struct cache_entry));
        if (!result)
            PANIC("MALLOC FAILURE: not enough memory for cache");
        list_push_back(&filesys_cache.cache_list, &result->elem);
        filesys_cache.cache_count++;
    }
    else 
    /* If the cache system is full, evict an existing cache to make room */
        result = cache_evict();
    if (result) {
        /* Initialize the created/evicted cache */
        result->sector = sector;
        result->dirty = dirty;
        result->accessed = true;
        result->open_count = 1;
        block_read(fs_device, sector, result->cache_block);
    }
    else {
        PANIC("EVICTION FAILURE: cache eviction undefined bug");
    }

    return result;
}

/*! Get a cache-block and load the sector content if not already
    loaded */
struct cache_entry *cache_get(block_sector_t sector, bool dirty) {
    struct cache_entry *result;

    lock_acquire(&filesys_cache.cache_lock);
    result = cache_readin(sector, dirty);
    lock_release(&filesys_cache.cache_lock);
    if (!dirty) {
        just_read = sector;
        sema_up(&read_ahead_lock);
    }
        
    return result;
}

/*! Evict a cache block from the cache list */
struct cache_entry *cache_evict(void) {
    struct cache_entry *result;
    /* Load the evict-pointer to start the clock algorithm */
    struct list_elem *curr = filesys_cache.evict_pointer;

    if (!curr)
        curr = list_begin(&filesys_cache.cache_list);
    while (curr && curr->next) {
        result = list_entry(curr, struct cache_entry, elem);
        if (result->accessed)
            result->accessed = false;
        else if (result->open_count == 0) {
            /* If no thread is actively accessing it */
            if (result->dirty) {
                /* Write the cache back if dirty */
                block_write(fs_device, result->sector, &result->cache_block);
            }
            /* Set the evict_pointer to the next element in the list */
            if (curr->next == list_end(&filesys_cache.cache_list))
                filesys_cache.evict_pointer = NULL;
            else
                filesys_cache.evict_pointer = list_next(curr);
            return result;
        }
        /* Iterate circularly through the list */
        if (curr->next == list_end(&filesys_cache.cache_list))
            curr = list_begin(&filesys_cache.cache_list);
        else
            curr = list_next(curr);
    }
    return NULL;
}

/* Write every dirty cache block back to disk and clear the dirty bit */
void cache_write_to_disk(bool shut) {
    struct list_elem *curr = list_begin(&filesys_cache.cache_list);
    struct cache_entry *curr_cache;
    struct list_elem *next;

    if (shut)
        just_read = -1;
    lock_acquire(&filesys_cache.cache_lock);
    while (curr && curr->next) {
        next = list_next(curr);
        curr_cache = list_entry(curr, struct cache_entry, elem);
        if (curr_cache->dirty) {
            block_write(fs_device, curr_cache->sector, 
                        &curr_cache->cache_block);
            curr_cache->dirty = false;
        }
        if (shut) {
            /* Used for freeing the cache system */
            list_remove(curr);
        }
        curr = next;
    }
    lock_release(&filesys_cache.cache_lock);
}

/* Main function for background write-behind */
void cache_write_background(void *aux) {
    while (true) {
        /* Wait for 5 sec */
        timer_sleep(CACHE_WRITE_TIME);
        cache_write_to_disk(false);
    }
}

/* Main function for background read-ahead */
void cache_read_ahead(void) {
    while (true) {
        sema_down(&read_ahead_lock);
        if (just_read < fs_device->size) {
            lock_acquire(&filesys_cache.cache_lock);
            struct cache_entry *ahead = cache_readin(just_read + 1, false);
            ahead->open_count--;
            lock_release(&filesys_cache.cache_lock);
        }
    }
}

