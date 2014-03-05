#include "vm/frame.h"
#include "vm/page.h"
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
#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

struct block *swap_block;
struct lock swap_lock;
struct bitmap *swap_bm;

void swap_init(void);
size_t swap_out(void *frame);
void swap_in(void *frame, size_t position);