#include <list.h>
#include "devices/block.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/cache.h"
#include "threads/thread.h"

void cache_init(void) {
    list_init(&filesys_cache.cache_list);
    lock_init(&filesys_cache.cache_lock);
    filesys_cache.cache_count = 0;
    filesys_cache.evict_pointer = NULL;
    thread_create("cache write background", PRI_MIN, 
                  cache_write_background, NULL);
}

struct cache_entry *cache_find(block_sector_t sector) {
    struct list_elem *curr = list_begin(&filesys_cache.cache_list);
    struct cache_entry *curr_cache;

    while (curr) {
        curr_cache = list_entry(curr, struct cache_entry, elem);
        if (curr_cache->sector == sector) {
            curr_cache->accessed = true;
            return curr_cache;
        }
        curr = list_next(curr);
    }
    return NULL;
}

struct cache_entry *cache_get(block_sector_t sector, bool dirty) {
    cache_entry *result;

    lock_acquire(&filesys_cache.cache_lock);
    if ((result = cache_find(sector)) != NULL) {
        result->dirty |= dirty;
        result->open_count++;
        lock_release(&filesys_cache.cache_lock);
        return result;
    }
    if (filesys_cache.cache_count < CACHE_MAXSIZE) {
        result = malloc(sizeof(cache_entry));
        if (!result)
            PANIC("MALLOC FAILURE: not enough memory for cache");
        list_push_back(&filesys_cache.cache_list, &result.elem);
        filesys_cache.cache_count++;
    }
    else 
        result = cache_evict();
    if (result) {
        result->sector = sector;
        result->dirty = dirty;
        result->accessed = true;
        result->open_count = 1;
        block_read(fs_device, sector, result->cach_block);
    }
    else {
        PANIC("EVICTION FAILURE: cache eviction undefined bug");
    }
    lock_release(&filesys_cache.cache_lock);
    return result;
}

struct cache_entry *cache_evict(void) {
    struct cache_entry *result;
    struct list_elem *curr = filesys_cache.evict_pointer;

    if (!curr)
        curr = list_begin(&filesys_cache.cache_list);
    while (curr) {
        result = list_entry(curr, struct cache_entry, elem);
        if (result->accessed)
            result->accessed = false;
        else {
            if (result->dirty)
                block_write(fs_device, result->sector, &result->cache_block);
            filesys_cache.eviction_pointer = list_next(curr);
            if (curr == list_end(&filesys_cache.cache_list)
                filesys_cache.eviction_pointer = NULL;
            return result;
        }
        if (curr == list_end(&filesys_cache.cache_list))
            curr = list_begin(&filesys_cache.cache_list);
        else
            curr = list_next(curr);
    }
    return NULL;
}

