#include <list.h>
#include "devices/block.h"
#include "devices/timer.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/cache.h"
#include "threads/thread.h"
#include "threads/malloc.h"

/*! Initialize the cache system */
void cache_init(void) {
    list_init(&filesys_cache.cache_list);
    lock_init(&filesys_cache.cache_lock);
    filesys_cache.cache_count = 0;
    filesys_cache.evict_pointer = NULL;
    /* Create ghost thread for periodical write-back */
    thread_create("cache write background", PRI_DEFAULT, 
                  cache_write_background, NULL);
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
    
    lock_acquire(&filesys_cache.cache_lock);
    /* If the cache already exists */
    if ((result = cache_find(sector)) != NULL) {
        result->dirty |= dirty;
        result->open_count++;
        result->accessed = true;
        lock_release(&filesys_cache.cache_lock);
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
        lock_release(&filesys_cache.cache_lock);
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

    result = cache_readin(sector, dirty);
    if (!dirty)
        cache_read_create(sector);
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
            free(curr_cache);
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
void cache_read_ahead(void *aux) {
    struct cache_entry *ahead = cache_readin(*((block_sector_t *)aux), false);
    ahead->open_count--;
    free(aux);
}

/* Create a read-ahead thread */
void cache_read_create(block_sector_t toread) {
    void *aux = malloc(sizeof(block_sector_t));
    if (aux)
        *((uint32_t *)aux) = toread + 1;
    else
        PANIC("MALLOC FAILURE: not enough memory");
    thread_create("cache read ahead", PRI_DEFAULT, 
                  cache_read_ahead, aux);
}
