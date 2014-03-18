#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

/*! Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define MAX_BLOCKS (BLOCK_SECTOR_SIZE / (sizeof (block_sector_t)) - 1)

static off_t inode_extend(struct inode *inode, off_t length);

/*! Returns the number of sectors to allocate for an inode SIZE
    bytes long. */
static inline size_t bytes_to_sectors(off_t size) {
    return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE);
}


/*! Returns the block device sector that contains byte offset POS
    within INODE.
    Returns -1 if INODE does not contain data for a byte at offset
    POS. */
static block_sector_t byte_to_sector(const struct inode *inode, off_t pos) {
    ASSERT(inode != NULL);
    if (pos < inode->data.length)
        return inode->data.start + pos / BLOCK_SECTOR_SIZE;
    else
        return -1;
}

/*! List of open inodes, so that opening a single inode twice
    returns the same `struct inode'. */
static struct list open_inodes;

/*! Initializes the inode module. */
void inode_init(void) {
    list_init(&open_inodes);
}

/*! Initializes an inode with LENGTH bytes of data and
    writes the new inode to sector SECTOR on the file system
    device.
    Returns true if successful.
    Returns false if memory or disk allocation fails. */
bool inode_create(block_sector_t sector, off_t length) {
    struct inode_disk *disk_inode = NULL;
    bool success = false;

    ASSERT(length >= 0);

    /* If this assertion fails, the inode structure is not exactly
       one sector in size, and you should fix that. */
    ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

    disk_inode = calloc(1, sizeof *disk_inode);
    if (disk_inode != NULL) {
        size_t sectors = bytes_to_sectors(length);
        disk_inode->length = length;
        disk_inode->magic = INODE_MAGIC;
        disk_inode->type = NON_FILE_INODE_DISK;
        if (free_map_allocate(sectors, &disk_inode->start)) {
            block_write(fs_device, sector, disk_inode);
            if (sectors > 0) {
                static char zeros[BLOCK_SECTOR_SIZE];
                size_t i;
              
                for (i = 0; i < sectors; i++) 
                    block_write(fs_device, disk_inode->start + i, zeros);
            }
            success = true; 
        }
        free(disk_inode);
    }
    return success;
}


/*! Initializes an inode with LENGTH bytes of data and
    writes the new inode to sector SECTOR on the file system
    device.
    Returns true if successful.
    Returns false if memory or disk allocation fails. */
bool inode_file_create(block_sector_t sector, off_t length) {
    struct inode_disk *disk_inode = NULL;
    bool success = false;
    size_t i = 0;
    off_t block_write_length;

    //printf("Creating file: %d with length %d\n\n", sector, length);
    
    ASSERT(length >= 0);

    /* If this assertion fails, the inode structure is not exactly
       one sector in size, and you should fix that. */
    ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

    disk_inode = calloc(1, sizeof *disk_inode);
    if (disk_inode != NULL) {
        size_t sectors = bytes_to_sectors(length);
        disk_inode->length = 0;
        disk_inode->magic = INODE_MAGIC;
        disk_inode->start = 0;
        disk_inode->type = FILE_INODE_DISK;
        /*if (free_map_allocate(sectors, &disk_inode->start)) {
            block_write(fs_device, sector, disk_inode);
            if (sectors > 0) {
                static char zeros[BLOCK_SECTOR_SIZE];
                size_t i;
              
                for (i = 0; i < sectors; i++) 
                    block_write(fs_device, disk_inode->start + i, zeros);
            }
            success = true; 
        }*/
        
        
        while (length > 0) {
            if (length >= BLOCK_SECTOR_SIZE) {
                length -= BLOCK_SECTOR_SIZE;
                block_write_length = BLOCK_SECTOR_SIZE;
            } else {
                block_write_length = length;
                length = 0;
            }
            if (!inode_alloc_block(disk_inode, block_write_length))
                return false;
        }
        block_write(fs_device, sector, disk_inode);
        success = true;
        free(disk_inode);
        
    }
    return success;
}

bool inode_alloc_block(struct inode_disk* head, off_t length) {
    block_sector_t block_i[MAX_BLOCKS + 1], prev_i[MAX_BLOCKS + 1];
    block_sector_t index, prev_index, new;
    size_t sectors = bytes_to_sectors(head->length);
    size_t index_in_block;
    off_t left;
    
    char zeros[BLOCK_SECTOR_SIZE];
    
    if (head->length % BLOCK_SECTOR_SIZE != 0) {
        left = BLOCK_SECTOR_SIZE - head->length % BLOCK_SECTOR_SIZE;
        if (length <= left) {
            head->length += length;
            return true;
        } else {
            length -= left;
            head->length += left;
        }

    }
        
    ASSERT((length <= BLOCK_SECTOR_SIZE) && (length > 0));
    if ((sectors % MAX_BLOCKS) == 0) {
        if (free_map_allocate(1, &index)) {
            block_i[MAX_BLOCKS] = 0;
            index_in_block = 0;
        } else {
            return false;    
        }
        //printf("allocating a new index sector: %d with %d\n", index, MAX_BLOCKS);
        
        if (head->start == 0)
            head->start = index;
        else {
            prev_index = inode_get_index_block(head, head->length);
            block_read(fs_device, prev_index, &prev_i);
            prev_i[MAX_BLOCKS] = index;
            block_write(fs_device, prev_index, &prev_i);
        }
    } else {
        index = inode_get_index_block(head, head->length);
        block_read(fs_device, index, &block_i);
        index_in_block = (sectors % MAX_BLOCKS);
    }
    
    if (!free_map_allocate(1, &new))
        return false;
    else {
        //printf("allocating new file sector: %d\n", new);
        block_i[index_in_block] = new;
        block_write(fs_device, index, &block_i);
        block_write(fs_device, new, &zeros);
    }
    //printf("nice\n");
    head->length += length;
    return true;
    
}

block_sector_t inode_get_index_block(struct inode_disk* head, off_t length) {
    block_sector_t block_i[MAX_BLOCKS + 1];
    size_t sectors = bytes_to_sectors(length);
    block_sector_t ret;
    if (length <= 0)
        return head->start;
    
    ret = head->start;
    block_read(fs_device, head->start, &block_i);
    while (sectors > MAX_BLOCKS) {
        ret = block_i[MAX_BLOCKS];
        block_read(fs_device, ret, &block_i);
        sectors -= MAX_BLOCKS;
    }
    //printf("Searching for %d, and found index block %d\n", length, ret);
    return ret;
}


/*! Reads an inode from SECTOR
    and returns a `struct inode' that contains it.
    Returns a null pointer if memory allocation fails. */
struct inode * inode_open(block_sector_t sector) {
    struct list_elem *e;
    struct inode *inode;

    /* Check whether this inode is already open. */
    for (e = list_begin(&open_inodes); e != list_end(&open_inodes);
         e = list_next(e)) {
        inode = list_entry(e, struct inode, elem);
        if (inode->sector == sector) {
            inode_reopen(inode);
            return inode; 
        }
    }

    /* Allocate memory. */
    inode = malloc(sizeof *inode);
    if (inode == NULL)
        return NULL;

    /* Initialize. */
    list_push_front(&open_inodes, &inode->elem);
    inode->sector = sector;
    inode->open_cnt = 1;
    inode->deny_write_cnt = 0;
    inode->removed = false;
    block_read(fs_device, inode->sector, &inode->data);
    return inode;
}

/*! Reopens and returns INODE. */
struct inode * inode_reopen(struct inode *inode) {
    if (inode != NULL)
        inode->open_cnt++;
    return inode;
}

/*! Returns INODE's inode number. */
block_sector_t inode_get_inumber(const struct inode *inode) {
    return inode->sector;
}

/*! Closes INODE and writes it to disk.
    If this was the last reference to INODE, frees its memory.
    If INODE was also a removed inode, frees its blocks. */
void inode_close(struct inode *inode) {
    //printf("Closing...\n");
    size_t i, sectors, cur_sec, cur_i, index_blocks;
    block_sector_t block_i[MAX_BLOCKS + 1];
    block_sector_t cur_block_i;
    
    /* Ignore null pointer. */
    if (inode == NULL)
        return;

    /* Release resources if this was the last opener. */
    if (--inode->open_cnt == 0) {
        /* Remove from inode list and release lock. */
        list_remove(&inode->elem);
 
        /* Deallocate blocks if removed. */
        if (inode->removed) {
            if (inode->data.type == NON_FILE_INODE_DISK) {
                free_map_release(inode->sector, 1);
                free_map_release(inode->data.start,
                                bytes_to_sectors(inode->data.length)); 
            } else if (inode->data.type == FILE_INODE_DISK) {
                sectors = bytes_to_sectors(inode_length(inode));
                index_blocks = sectors / MAX_BLOCKS;
                if (sectors % MAX_BLOCKS != 0)
                    ++index_blocks;
                cur_i = 0;
                cur_sec = 0;
                cur_block_i = inode_get_index_block(&inode->data, 1);
                
                while (cur_i < index_blocks && cur_sec <= sectors){
                    block_read(fs_device, cur_block_i, &block_i);
                    for (i = 0; i < MAX_BLOCKS; i++) {
                        ++ cur_sec;
                        if (cur_sec > sectors)
                            break;
                        free_map_release(block_i[i], 1);
                    }
                    cur_block_i = block_i[MAX_BLOCKS];
                    free_map_release(cur_block_i, 1);
                }
                
            }
        }

        free(inode); 
    }
}

/*! Marks INODE to be deleted when it is closed by the last caller who
    has it open. */
void inode_remove(struct inode *inode) {
    ASSERT(inode != NULL);
    inode->removed = true;
}

/*! Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t inode_read_at(struct inode *inode, void *buffer_, off_t size, off_t offset) {
    //printf("Reading\n");
    uint8_t *buffer = buffer_;
    off_t bytes_read = 0;
    struct cache_entry *c;
    size_t index_in_block;
    block_sector_t block_i[MAX_BLOCKS + 1];
    block_sector_t sector_idx;
    off_t temp = size;

    if (inode_length(inode) < offset + size)
        return 0;

    while (size > 0) {
        if (inode->data.type == NON_FILE_INODE_DISK) {
            /* Disk sector to read, starting byte offset within sector. */
            sector_idx = byte_to_sector (inode, offset);
            int sector_ofs = offset % BLOCK_SECTOR_SIZE;

            /* Bytes left in inode, bytes left in sector, lesser of the two. */
            off_t inode_left = inode_length(inode) - offset;
            int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
            int min_left = inode_left < sector_left ? inode_left : sector_left;

            /* Number of bytes to actually copy out of this sector. */
            int chunk_size = size < min_left ? size : min_left;
            if (chunk_size <= 0)
                break;

            c = cache_get(sector_idx, false);
            memcpy(buffer + bytes_read, (uint8_t *)&c->cache_block + sector_ofs, 
                chunk_size);
        
            /* Advance. */
            size -= chunk_size;
            offset += chunk_size;
            bytes_read += chunk_size;
        } else if (inode->data.type == FILE_INODE_DISK) {
            sector_idx = inode_get_index_block(&inode->data, 
                                               offset);
            if (offset % (BLOCK_SECTOR_SIZE * MAX_BLOCKS) == 0)
                index_in_block = 0;
            else
                index_in_block = (bytes_to_sectors(offset + 1) - 1) % MAX_BLOCKS;
            
            int sector_ofs = offset % BLOCK_SECTOR_SIZE;
            
            /* Bytes left in inode, bytes left in sector, lesser of the two. */
            off_t inode_left = inode_length(inode) - offset;
            int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
            int min_left = inode_left < sector_left ? inode_left : sector_left;
            
            /* Number of bytes to actually copy out of this sector. */
            int chunk_size = size < min_left ? size : min_left;
            if (chunk_size <= 0)
                break;
            
            //printf("reading from index block %d at index %d\n", sector_idx, index_in_block);
            
            block_read(fs_device, sector_idx, &block_i);
            
            //printf("reading from %d\n", block_i[index_in_block]);
            
            c = cache_get(block_i[index_in_block], false);
            memcpy(buffer + bytes_read, (uint8_t *)&c->cache_block + sector_ofs, 
                   chunk_size);
            
            /* Advance. */
            size -= chunk_size;
            offset += chunk_size;
            bytes_read += chunk_size;
        }
    }
    //printf("Done reading. %d; should be %d\n", bytes_read, temp);
    return bytes_read;
}

/*! Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
    Returns the number of bytes actually written, which may be
    less than SIZE if end of file is reached or an error occurs.
    (Normally a write at end of file would extend the inode, but
    growth is not yet implemented.) */
off_t inode_write_at(struct inode *inode, const void *buffer_, off_t size, off_t offset) {
    //printf("Writing.\n");
    const uint8_t *buffer = buffer_;
    off_t bytes_written = 0;
    struct cache_entry *c;
    size_t index_in_block;
    block_sector_t block_i[MAX_BLOCKS + 1];
    
    if (inode->deny_write_cnt)
        return 0;

    if (inode_length(inode) < offset + size)
        inode_extend(inode, offset + size);

    while (size > 0) {
        if (inode->data.type == NON_FILE_INODE_DISK) {
            /* Sector to write, starting byte offset within sector. */
            block_sector_t sector_idx = byte_to_sector(inode, offset);
            int sector_ofs = offset % BLOCK_SECTOR_SIZE;
            /* Bytes left in inode, bytes left in sector, lesser of the two. */
            off_t inode_left = inode_length(inode) - offset;
            int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
            int min_left = inode_left < sector_left ? inode_left : sector_left;
            /* Number of bytes to actually write into this sector. */
            int chunk_size = size < min_left ? size : min_left;
            if (chunk_size <= 0)
                break;
            
            c = cache_get(sector_idx, true);
            memcpy((uint8_t *)&c->cache_block + sector_ofs, buffer + bytes_written,
                chunk_size);
            c->dirty = true;

            /* Advance. */
            size -= chunk_size;
            offset += chunk_size;
            bytes_written += chunk_size;
        } else if (inode->data.type == FILE_INODE_DISK) {
            block_sector_t sector_idx = inode_get_index_block(&inode->data, offset);
            if (offset % (BLOCK_SECTOR_SIZE * MAX_BLOCKS) == 0)
                index_in_block = 0;
            else {
                index_in_block = (bytes_to_sectors(offset + 1) - 1) % MAX_BLOCKS;
                //printf("b2s: %d %d %d\n", bytes_to_sectors(offset), index_in_block, 0%MAX_BLOCKS);
                
            }
            int sector_ofs = offset % BLOCK_SECTOR_SIZE;
            
            /* Bytes left in inode, bytes left in sector, lesser of the two. */
            off_t inode_left = inode_length(inode) - offset;
            int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
            int min_left = inode_left < sector_left ? inode_left : sector_left;
            
            /* Number of bytes to actually copy out of this sector. */
            int chunk_size = size < min_left ? size : min_left;
            if (chunk_size <= 0)
                break;
            
             //printf("writing to index block %d at index %d at offset %d\n", 
             //       sector_idx, index_in_block, offset);
            
            block_read(fs_device, sector_idx, &block_i);
            
            //printf("Writing to %d\n", block_i[index_in_block]);
            
            c = cache_get(block_i[index_in_block], false);
            memcpy((uint8_t *)&c->cache_block + sector_ofs, buffer + bytes_written,
                chunk_size);
            c->dirty = true;
            
            /* Advance. */
            size -= chunk_size;
            offset += chunk_size;
            bytes_written += chunk_size;
        }
    }
    //printf("done writing.\n");
    return bytes_written;
}

/*! Disables writes to INODE.
    May be called at most once per inode opener. */
void inode_deny_write (struct inode *inode) {
    inode->deny_write_cnt++;
    ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

/*! Re-enables writes to INODE.
    Must be called once by each inode opener who has called
    inode_deny_write() on the inode, before closing the inode. */
void inode_allow_write (struct inode *inode) {
    ASSERT(inode->deny_write_cnt > 0);
    ASSERT(inode->deny_write_cnt <= inode->open_cnt);
    inode->deny_write_cnt--;
}

/*! Returns the length, in bytes, of INODE's data. */
off_t inode_length(const struct inode *inode) {
    return inode->data.length;
}

static off_t inode_extend(struct inode *inode, off_t length) {
    static char zeros[BLOCK_SECTOR_SIZE];
    ASSERT(inode && inode->data);
    struct inode_disk *head = &inode->data;
    off_t old_len = head->length;
    uint32_t new_blocks = bytes_to_sectors(old_len + length) -
                          bytes_to_sectors(old_len);
    off_t chunk_size, new_length;

    new_length = length + old_len;

    while (length > 0) {
        chunk_size = length >= BLOCK_SECTOR_SIZE ? BLOCK_SECTOR_SIZE : length;
        if (!inode_alloc_block(head, chunk_size))
            PANIC("not enough memory for file extension!");
        length -= chunk_size;
    }
    return new_length;
}