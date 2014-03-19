#include <list.h>
#include "devices/block.h"
#include "devices/timer.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/cache.h"
#include "threads/thread.h"
#include "threads/malloc.h"

void cache_init(void) {
    list_init(&filesys_cache.cache_list);
    lock_init(&filesys_cache.cache_lock);
    filesys_cache.cache_count = 0;
    filesys_cache.evict_pointer = NULL;
    thread_create("cache write background", PRI_DEFAULT, 
                  cache_write_background, NULL);
}

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

static struct cache_entry *cache_readin(block_sector_t sector, bool dirty) {

    struct cache_entry *result;
    
    if ((result = cache_find(sector)) != NULL) {
        //printf("found cache %d\n\n", result->sector);
        //printf("%d %x %x %x %x read cache found %d\n\n", result->dirty, result->cache_block[0], result->cache_block[1], result->cache_block[2], result->cache_block[3], result->sector);
         
        result->dirty |= dirty;
        result->open_count++;
        lock_release(&filesys_cache.cache_lock);
        return result;
    }
    if (filesys_cache.cache_count < CACHE_MAXSIZE) {
        result = malloc(sizeof(struct cache_entry));
        if (!result)
            PANIC("MALLOC FAILURE: not enough memory for cache");
        list_push_back(&filesys_cache.cache_list, &result->elem);
        filesys_cache.cache_count++;
    }
    else 
        result = cache_evict();
    if (result) {
        result->sector = sector;
        result->dirty = dirty;
        result->accessed = true;
        result->open_count = 1;
        block_read(fs_device, sector, result->cache_block);
        //printf("%d %x %x %x %x read cache %d\n\n", result->dirty, result->cache_block[0], result->cache_block[1], result->cache_block[2], result->cache_block[3], result->sector);
            
    }
    else {
        PANIC("EVICTION FAILURE: cache eviction undefined bug");
    }

    return result;
}

struct cache_entry *cache_get(block_sector_t sector, bool dirty) {
    struct cache_entry *result;

    lock_acquire(&filesys_cache.cache_lock);
    result = cache_readin(sector, dirty);
    lock_release(&filesys_cache.cache_lock);
    cache_read_create(sector);
    return result;
}

struct cache_entry *cache_evict(void) {
    struct cache_entry *result;
    struct list_elem *curr = filesys_cache.evict_pointer;

    if (!curr)
        curr = list_begin(&filesys_cache.cache_list);
    while (curr && curr->next) {
        result = list_entry(curr, struct cache_entry, elem);
        if (result->accessed)
            result->accessed = false;
        else {
            if (result->dirty) {
              //  printf("writing back to sector %d\n\n", result->sector);
                //printf("%x %x %x %x written cache\n\n", result->cache_block[0], result->cache_block[1], result->cache_block[2], result->cache_block[3]);
            
                block_write(fs_device, result->sector, &result->cache_block);
            }
            if (curr->next == list_end(&filesys_cache.cache_list))
                filesys_cache.evict_pointer = NULL;
            else
                filesys_cache.evict_pointer = list_next(curr);
            return result;
        }
        if (curr->next == list_end(&filesys_cache.cache_list))
            curr = list_begin(&filesys_cache.cache_list);
        else
            curr = list_next(curr);
    }
    return NULL;
}

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
            list_remove(curr);
            free(curr_cache);
        }
        curr = next;
    }
    lock_release(&filesys_cache.cache_lock);
}

void cache_write_background(void *aux) {
    while (true) {
        timer_sleep(CACHE_WRITE_TIME);
        cache_write_to_disk(false);
    }
}

void cache_read_ahead(void *aux) {
    cache_readin(*((block_sector_t *)aux), false);
    free(aux);
}

void cache_read_create(block_sector_t toread) {
    void *aux = malloc(sizeof(block_sector_t));
    if (aux)
        *((uint32_t *)aux) = toread + 1;
    else
        PANIC("MALLOC FAILURE: not enough memory");
    thread_create("cache read ahead", PRI_MIN, 
                  cache_read_ahead, aux);
}
