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
    
    /* Allocate a new disk_inode for this file. */
    disk_inode = calloc(1, sizeof *disk_inode);
    if (disk_inode != NULL) {
        
        /* Find out how many sectors this length needs. */
        size_t sectors = bytes_to_sectors(length);
        
        /* Initialize the fields of disk_inode. */
        disk_inode->length = length;
        disk_inode->magic = INODE_MAGIC;
        disk_inode->type = NON_FILE_INODE_DISK;
        
        if (free_map_allocate(sectors, &disk_inode->start)) {
            /* For non-file or dir type of inode, we will just assign 
             * consecutive sectors. */
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


/*! Initializes a file inode with LENGTH bytes of data and
    writes the new inode to sector SECTOR on the file system
    device. This group of sector has type FILE_INODE_DISK.
    Returns true if successful.
    Returns false if memory or disk allocation fails. */
bool inode_file_create(block_sector_t sector, off_t length) {
    struct inode_disk *disk_inode = NULL;
    bool success = false;
    off_t block_write_length;
    
    ASSERT(length >= 0);

    /* If this assertion fails, the inode structure is not exactly
       one sector in size, and you should fix that. */
    ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

    /* Allocate a new disk_inode for this file. */
    disk_inode = calloc(1, sizeof *disk_inode);
    if (disk_inode != NULL) {
        /* Find out how many sectors this length needs. */
        //size_t sectors = bytes_to_sectors(length);
        
        /* Initialize the fields of disk_inode. */
        disk_inode->length = 0;
        disk_inode->magic = INODE_MAGIC;
        disk_inode->start = 0;
        disk_inode->type = FILE_INODE_DISK;
        
        /* If length > 0, then we need to allocate new sectors. */
        while (length > 0) {
            /* Cut length sector-size by sector-size*/
            if (length >= BLOCK_SECTOR_SIZE) {
                length -= BLOCK_SECTOR_SIZE;
                block_write_length = BLOCK_SECTOR_SIZE;
            } else {
                block_write_length = length;
                length = 0;
            }
            
            /* Allocate a new sector. */
            if (!inode_alloc_block(disk_inode, block_write_length))
                return false;
        }
        
        /* Write the disk_inode to disk sector. */
        block_write(fs_device, sector, disk_inode);
        /*Set return flag to true.*/
        success = true;
        /*Free the allocated disk_inode, as we have written it disk already.*/
        free(disk_inode);
        
    }
    return success;
}

/*! Initializes a directory inode with LENGTH bytes of data and
    writes the new inode to sector SECTOR on the file system
    device. This group of sector has type DIR_INODE_DISK.
    Returns true if successful.
    Returns false if memory or disk allocation fails. */
bool inode_dir_create(block_sector_t sector, off_t length) {
    struct inode_disk *disk_inode = NULL;
    bool success = false;
    off_t block_write_length;

    
    ASSERT(length >= 0);

    /* If this assertion fails, the inode structure is not exactly
       one sector in size, and you should fix that. */
    ASSERT(sizeof *disk_inode == BLOCK_SECTOR_SIZE);

    /* If this assertion fails, the inode structure is not exactly
       one sector in size, and you should fix that. */
    disk_inode = calloc(1, sizeof *disk_inode);
    if (disk_inode != NULL) {
        
        /* Find out how many sectors this length needs. */
        //size_t sectors = bytes_to_sectors(length);
        
         /* Initialize the fields of disk_inode. */
        disk_inode->length = 0;
        disk_inode->magic = INODE_MAGIC;
        disk_inode->start = 0;
        disk_inode->type = DIR_INODE_DISK;
        
        /* If length > 0, then we need to allocate new sectors. */
        while (length > 0) {
            /* Cut length sector-size by sector-size*/
            if (length >= BLOCK_SECTOR_SIZE) {
                length -= BLOCK_SECTOR_SIZE;
                block_write_length = BLOCK_SECTOR_SIZE;
            } else {
                block_write_length = length;
                length = 0;
            }
            
            /* Allocate a new sector. */
            if (!inode_alloc_block(disk_inode, block_write_length))
                return false;
        }
        
        /* Write the disk_inode to disk sector. */
        block_write(fs_device, sector, disk_inode);
        /*Set return flag to true.*/
        success = true;
        /*Free the allocated disk_inode, as we have written it disk already.*/
        free(disk_inode);
        
    }
    return success;
}

/*! Allocate a new sector to a file/directory inode. 
 *  Return true if successful; False if not. */

bool inode_alloc_block(struct inode_disk* head, off_t length) {
    block_sector_t block_i[MAX_BLOCKS + 1], prev_i[MAX_BLOCKS + 1];
    block_sector_t index, prev_index, new;
    
    /* Find out the number of sector this inode already has. */
    size_t sectors = bytes_to_sectors(head->length);
    
    /* index of the new allocated sector in its corresponding
     * index sector. */ 
    size_t index_in_block;
    
    /* Stores the remaining unused bytes for the inode's last allocated 
     * sector. */
    off_t left;
    
    /* Flag for allocating a new starting index sector for an inode. */
    bool new_start_block = false;
    
    /* Flag for allocating a new index block,
     * but not the starting index block. */
    bool new_index_block = false;

    /* Zero-filled data to rewrite the newly allocated sector. */
    char zeros[BLOCK_SECTOR_SIZE];
    memset(&zeros, 0, BLOCK_SECTOR_SIZE);
    
    /* If there is unused bytes in the last allocated sector. */
    if (head->length % BLOCK_SECTOR_SIZE != 0) {
        /* Find out the remaining bytes*/
        left = BLOCK_SECTOR_SIZE - head->length % BLOCK_SECTOR_SIZE;
        if (length <= left) {
            /* If remaining bytes larger than the length, then we do not
             * have to allocate another sector. Just update the length
             * in inode. */
            head->length += length;
            return true;
        } else {
            /* Otherwise we just need to allocate the difference between 
             * the two. Update inode's lenghth with left. */
            length -= left;
            head->length += left;
        }

    }
        
    ASSERT((length <= BLOCK_SECTOR_SIZE) && (length > 0));
    
    if ((sectors % MAX_BLOCKS) == 0) {
        /* If we need to allocated a new index sector. */
        if (free_map_allocate(1, &index)) {
            /* If we got a sector for this index block, then
             * initialize by setting the last block_size_t as NULL.
             * (which is the pointer to a future next index block) */
            block_i[MAX_BLOCKS] = 0;
            
            /* Set the index in sector as 0. */
            index_in_block = 0;
        } else {
            return false;    
        }
        
        if (head->start == 0){
            /* If we have not assigned a starting index to the inode yet, then
             * assign the index sector number to inode's start. */
            head->start = index;
            /* Set the start-index-sector flag as true. */
            new_start_block = true;
        }
        else {
            /* Otherwise, we will append this new index sector to the last 
             * index sector of the inode. */
            
            /* Look for the block_sector_t of the last index sector. */
            prev_index = inode_get_index_block(head, head->length - 1);
            /* Read the data from the last index sector. */
            block_read(fs_device, prev_index, &prev_i);
            /* Set the last block_sector_t as the new index sector's number.*/
            prev_i[MAX_BLOCKS] = index;
            /* Write the index data back to disk.*/
            block_write(fs_device, prev_index, &prev_i);
            /* Set the new index block to true. */
            new_index_block = true;
        }
    } else {
        /* As we do not need a new index block in this case. */
        /* Find the last index block. */
        index = inode_get_index_block(head, head->length - 1);
        /* Read index data from disk. */
        block_read(fs_device, index, &block_i);
        /* Set the in-sector index according number of sectors in inode. */
        index_in_block = (sectors % MAX_BLOCKS);
    }
    
    if (!free_map_allocate(1, &new)){
        /* Cannot allocate a new sector. */
        if (new_start_block){
            /* As we allocated a new start index sector for nothing. 
             * Free everything: map_release the index sector. Set the start
             * of inode back to 0. */
            free_map_release(index, 1);
            head->start = 0;
        }
        
        if (new_index_block){
            /* As we allocated a new index sector for nothing. 
             * Free everything: map_release the index sector. Set the 
             * "pointer" in the last index sector back to NULL (or 0). */
            prev_i[MAX_BLOCKS] = 0;
            free_map_release(index, 1);
        }
        return false;
    }
    else {
        /* Store the new sector's number on to the index sector. */
        block_i[index_in_block] = new;
        /* Write the index data back to disk. */
        block_write(fs_device, index, &block_i);
        /* Write zeros to the new sector. */
        block_write(fs_device, new, &zeros);
    }
    /* Update the length of the inode. */
    head->length += length;
    return true;
    
}

/*! Get the index sector number of an inode, given the offset. */
block_sector_t inode_get_index_block(struct inode_disk* head, off_t length) {
    /* The array to read the index data*/
    block_sector_t block_i[MAX_BLOCKS + 1];
    
    /* The index sector number*/
    block_sector_t ret;
    
    /* Find number of sectors needed to fit such length*/
    size_t sectors = bytes_to_sectors(length + 1);
    
    /* If offset is less than or equal to zero, than we will just rest return 
     * the start index of the inode. */
    if (length <= 0)
        return head->start;
    
    /* Iterate through the index blocks, until we are at the index block of 
     * the given length's corresponding number of sectors.*/
    ret = head->start;
    block_read(fs_device, head->start, &block_i);
    while (sectors > MAX_BLOCKS) {
        ret = block_i[MAX_BLOCKS];
        block_read(fs_device, ret, &block_i);
        sectors -= MAX_BLOCKS;
    }
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
    lock_init(&inode->lock);
    block_read(fs_device, inode->sector, &inode->data);
    inode->read_length = inode->data.length;
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

    size_t i,sectors, cur_sec, cur_i, index_blocks;
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
                /* Remove the consecutive sectors and the inode. */
                free_map_release(inode->sector, 1);
                free_map_release(inode->data.start,
                                bytes_to_sectors(inode->data.length)); 
            } else if (inode->data.start != 0) {
                /* We only need to free sectors if we have allocated the
                 * start index sector for the inode.*/
                
                /* Get number of sectors allocated in this inode.*/
                sectors = bytes_to_sectors(inode_length(inode));
                
                /* Get the number of index sectors in this inode. */
                index_blocks = sectors / MAX_BLOCKS;
                if (sectors % MAX_BLOCKS != 0)
                    ++index_blocks;
                
                /* Index for index sectors*/
                cur_i = 0;
                /* Index for data sectors. */
                cur_sec = 0;
                /* block_sector_t of the current index sector we are 
                 * iterating. */
                cur_block_i = inode->data.start;
                
                /* Iterate through all index sectors and data sectors. */
                while (cur_i < index_blocks && cur_sec <= sectors){
                    /* Read the index data. */
                    block_read(fs_device, cur_block_i, &block_i);
                    for (i = 0; i < MAX_BLOCKS; i++) {
                        /* Free the data sectors stored on the index sector.*/
                        ++ cur_sec;
                        if (cur_sec > sectors)
                            break;
                        free_map_release(block_i[i], 1);

                    }
                    
                    /* Free this index sector. */
                    free_map_release(cur_block_i, 1);
                    /* Get the nex index sector's number. */
                    cur_block_i = block_i[MAX_BLOCKS];
                    ++cur_i;
                    
                }
                
                /* Free the inode on the disk. */
                free_map_release(inode->sector, 1);
            }
        } else {
            /* As we are not removing this file, we will just write the inode 
             * back to disk.*/
            block_write(fs_device, inode->sector, &inode->data);
        }
        
        /* Just free the inode struct if the file/dir is not removed. */
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
off_t inode_read_at(struct inode *inode, void *buffer_, off_t size, 
                    off_t offset) {
    
    /* Set a pointer iterator for buffer. */
    uint8_t *buffer = buffer_;
    
    /* Bytes read from inode. */
    off_t bytes_read = 0;
    
    /* Cache entry of the data read. */
    struct cache_entry *c;
    
    /* In-sector index of the sector read from disk on its corresponding 
     * index sector. */
    size_t index_in_block;
    
    /* Index sector data buffer */
    block_sector_t block_i[MAX_BLOCKS + 1];
    
    /* Sector number of the data read from disk*/
    block_sector_t sector_idx;

    if (inode->read_length < offset + size)
        return 0;

    while (size > 0) {
        if (inode->data.type == NON_FILE_INODE_DISK) {
            /* Disk sector to read, starting byte offset within sector. */
            sector_idx = byte_to_sector (inode, offset);
            int sector_ofs = offset % BLOCK_SECTOR_SIZE;

            /* Bytes left in inode, bytes left in sector, lesser of the two.*/
            off_t inode_left = inode_length(inode) - offset;
            int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
            int min_left = 
                 inode_left < sector_left ? inode_left : sector_left;

            /* Number of bytes to actually copy out of this sector. */
            int chunk_size = size < min_left ? size : min_left;
            if (chunk_size <= 0)
                break;
            
            /* Cache in the data*/
            c = cache_get(sector_idx, false);
            /* Copy the data from cache to buffer*/
            memcpy(buffer + bytes_read, 
                   (uint8_t *)&c->cache_block + sector_ofs, chunk_size);
            c->open_count--;
        
            /* Advance. */
            size -= chunk_size;
            offset += chunk_size;
            bytes_read += chunk_size;
        } else  {
            /* Get the index sector of the given inode and offset. */
            sector_idx = inode_get_index_block(&inode->data, 
                                               offset);
            
            /* Get the index of the sector in its corresponding index 
             * sector. */
            if (offset % (BLOCK_SECTOR_SIZE * MAX_BLOCKS) == 0)
                index_in_block = 0;
            else
                index_in_block = 
                    (bytes_to_sectors(offset + 1) - 1) % MAX_BLOCKS;
            
            int sector_ofs = offset % BLOCK_SECTOR_SIZE;
            
            /* Bytes left in inode, bytes left in sector, lesser of the two.*/
            off_t inode_left = inode_length(inode) - offset;
            int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
            int min_left = 
                inode_left < sector_left ? inode_left : sector_left;
            
            /* Number of bytes to actually copy out of this sector. */
            int chunk_size = size < min_left ? size : min_left;
            if (chunk_size <= 0)
                break;
            
            /* Read the index data from the index sector*/
            block_read(fs_device, sector_idx, &block_i);
            
            /* Cache in*/
            c = cache_get(block_i[index_in_block], false);
            /* Copy data from cache to buffer */
            memcpy(buffer + bytes_read, 
                   (uint8_t *)&c->cache_block + sector_ofs, 
                   chunk_size);
            
            c->open_count--;
            
            /* Advance. */
            size -= chunk_size;
            offset += chunk_size;
            bytes_read += chunk_size;
        }
    }

    return bytes_read;
}

/*! Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
    Returns the number of bytes actually written, which may be
    less than SIZE if end of file is reached or an error occurs.
    (Normally a write at end of file would extend the inode, but
    growth is not yet implemented.) */
off_t inode_write_at(struct inode *inode, const void *buffer_, off_t size, 
                     off_t offset) {
    /* Set a pointer iterator for buffer. */
    const uint8_t *buffer = buffer_;
    
    /* Bytes read from inode. */
    off_t bytes_written = 0;
    
    /* Cache entry */
    struct cache_entry *c;
    
    /* In-sector index of the sector read from disk on its corresponding 
     * index sector. */
    size_t index_in_block;
    
    /* Sector number of the data read from disk*/
    block_sector_t block_i[MAX_BLOCKS + 1];
    
    /* If the inode does not allow right, then just return.*/
    if (inode->deny_write_cnt)
        return 0;

    /* lock the inode for extending the file*/
    if (inode_length(inode) < offset + size) {
        lock_acquire(&inode->lock);
        if (!inode_extend(inode, offset + size)) {
            lock_release(&inode->lock);
            return 0;
        }
        lock_release(&inode->lock);
    }

    while (size > 0) {
        if (inode->data.type == NON_FILE_INODE_DISK) {
            /* Sector to write, starting byte offset within sector. */
            block_sector_t sector_idx = byte_to_sector(inode, offset);
            int sector_ofs = offset % BLOCK_SECTOR_SIZE;
            /* Bytes left in inode, bytes left in sector, lesser of the two.*/
            off_t inode_left = inode_length(inode) - offset;
            int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
            int min_left = 
                inode_left < sector_left ? inode_left : sector_left;
            /* Number of bytes to actually write into this sector. */
            int chunk_size = size < min_left ? size : min_left;
            if (chunk_size <= 0)
                break;
            
            /* Cache in*/
            c = cache_get(sector_idx, false);
            /* Copy buffer to cache*/
            memcpy((uint8_t *)&c->cache_block + sector_ofs, 
                   buffer + bytes_written,
                chunk_size);
            
            c->dirty = true;
            c->open_count--;

            /* Advance. */
            size -= chunk_size;
            offset += chunk_size;
            bytes_written += chunk_size;
            inode->read_length += chunk_size;
        } else {
            /* Get the index sector of the given inode and offset. */
            block_sector_t sector_idx = 
                inode_get_index_block(&inode->data, offset);
            
            /* Get the index of the sector in its corresponding index 
             * sector. */
            if (offset % (BLOCK_SECTOR_SIZE * MAX_BLOCKS) == 0)
                index_in_block = 0;
            else {
                index_in_block = 
                    (bytes_to_sectors(offset + 1) - 1) % MAX_BLOCKS;   
            }
            
            
            int sector_ofs = offset % BLOCK_SECTOR_SIZE;
            
            /* Bytes left in inode, bytes left in sector, lesser of the two.*/
            off_t inode_left = inode_length(inode) - offset;
            int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
            int min_left = 
                inode_left < sector_left ? inode_left : sector_left;
            
            /* Number of bytes to actually copy out of this sector. */
            int chunk_size = size < min_left ? size : min_left;
            if (chunk_size <= 0)
                break;

            /* Read the index data from the index sector*/
            block_read(fs_device, sector_idx, &block_i);
            
            /* Cache in*/
            c = cache_get(block_i[index_in_block], true);
            /* Copy data from cache to buffer */
            memcpy((uint8_t *)&c->cache_block + sector_ofs, 
                   buffer + bytes_written,
                chunk_size);
            
            c->dirty = true;
            c->open_count--;
            
            /* Advance. */
            size -= chunk_size;
            offset += chunk_size;
            bytes_written += chunk_size;
            inode->read_length += chunk_size;
        }
    }

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

/*! Extending a file or directory beyond its original length. */
static off_t inode_extend(struct inode *inode, off_t length) {
    ASSERT(inode != NULL);
    
    /* Get the inode disk of this inode*/
    struct inode_disk *head = &inode->data;
    
    /* The original length of the inode*/
    off_t old_len = head->length;
    ASSERT(length > old_len);
    
    /* Get number of new sectors required for the extension.*/
    uint32_t new_blocks = bytes_to_sectors(length) -
                          bytes_to_sectors(old_len);
    
    off_t chunk_size, len_extended;
    len_extended = old_len;
    
    /* Get the extension length */
    length -= old_len;
    
    while (length > 0) {
        /* Cut the length block-size by block-size*/
        chunk_size = length >= BLOCK_SECTOR_SIZE ? BLOCK_SECTOR_SIZE : length;
        
        /* Allocate a new sector*/
        if (!inode_alloc_block(head, chunk_size))
            return 0;
        
        /* Advance*/
        length -= chunk_size;
        len_extended += chunk_size;
    }
    
    /* Write the update inode disk back to disk*/
    block_write(fs_device, inode->sector, head);
    return len_extended;
}
