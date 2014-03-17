#ifndef FILESYSCACHE_H
#define FILESYSCACHE_H

#include <list.h>
#include "devices/block.h"
#include "threads/synch.h"
#include "devices/timer.h"

#define CACHE_MAXSIZE 64
#define CACHE_WRITE_TIME 5*TIMER_FREQ


struct cache_entry {
    block_sector_t sector;
    uint8_t cache_block[BLOCK_SECTOR_SIZE];

    struct list_elem elem;              /* List element */
    int open_count;                     /* Number of processes opening */
    bool accessed;                      /* Whether cache has been accessed */
    bool dirty;                         /* Whether cache is dirty */
};

struct cache_system {
    struct list cache_list;             /* List of cache blocks */
    struct lock cache_lock;             /* Global cache lock */
    uint32_t cache_count;               /* Number of cache blocks allocated */
    struct list_elem *evict_pointer;    /* For implementing clock algorithm */
};

struct cache_system filesys_cache;

void cache_init(void);
struct cache_entry * cache_find(block_sector_t sector);
struct cache_entry * cache_get(block_sector_t sector, bool dirty);
struct cache_entry * cache_evict(void);

void cache_write_to_disk(bool shut);
void cache_write_background(void *aux);
void cache_read_ahead(void *aux);
void cache_read_create(block_sector_t toread);

#endif