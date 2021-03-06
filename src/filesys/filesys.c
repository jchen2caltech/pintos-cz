#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"
#include "threads/thread.h"
#include "threads/synch.h"

/*! Partition that contains the file system. */
struct block *fs_device;

static void do_format(void);

/*! Initializes the file system module.
    If FORMAT is true, reformats the file system. */
void filesys_init(bool format) {
    fs_device = block_get_role(BLOCK_FILESYS);
    if (fs_device == NULL)
        PANIC("No file system device found, can't initialize file system.");

    inode_init();
    free_map_init();
    cache_init();

    if (format) 
        do_format();

    free_map_open();
}

/*! Shuts down the file system module, writing any unwritten data to disk. */
void filesys_done(void) {
    cache_write_to_disk(true);
    free_map_close();
}

/*! Creates a file named NAME with the given INITIAL_SIZE under the given 
    directory. Returns true if successful, false otherwise.  Fails if a file
    named NAME already exists, or if internal memory allocation fails. */
bool filesys_dir_create(const char *name, off_t initial_size, struct dir* dir){
    block_sector_t inode_sector = 0;
    
    /* Check for valid directory; 
     * then allocate a new sector for the new file
     * Create the file's inode
     * add the file too the directory */
    
    bool success = (dir != NULL &&
                    free_map_allocate(1, &inode_sector) &&
                    inode_file_create(inode_sector, initial_size) &&
                    dir_add(dir, name, inode_sector));
    
    /* If unsucessful, then free the allocated sector number. */
    if (!success && inode_sector != 0) 
        free_map_release(inode_sector, 1);

    return success;
}


/*! Deletes the file named NAME under the given directory.  
    Returns true if successful, false on failure.
    Fails if no file named NAME exists, or if an internal memory allocation
    fails. */
bool filesys_dir_remove(const char *name, struct dir* dir){
    bool success;

    success = dir != NULL && dir_remove(dir, name);

    return success;
}

/*! Creates a file named NAME with the given INITIAL_SIZE.  Returns true if
    successful, false otherwise.  Fails if a file named NAME already exists,
    or if internal memory allocation fails. */
bool filesys_create(const char *name, off_t initial_size) {
    block_sector_t inode_sector = 0;
    struct dir *dir;
    struct thread* t = thread_current();
    
    if (t->cur_dir == NULL){
        t->cur_dir = dir_open_root();
        dir = dir_open_root();
    } else {
        dir = dir_reopen(t->cur_dir);    
    }
    
    bool success = (dir != NULL &&
                    free_map_allocate(1, &inode_sector) &&
                    inode_file_create(inode_sector, initial_size) &&
                    dir_add(dir, name, inode_sector));
    if (!success && inode_sector != 0) 
        free_map_release(inode_sector, 1);
    dir_close(dir);

    return success;
}

/*! Opens the file with the given NAME.  Returns the new file if successful
    or a null pointer otherwise.  Fails if no file named NAME exists,
    or if an internal memory allocation fails. */
struct file * filesys_open(const char *name) {
    struct inode *inode = NULL;
    struct dir *dir;
    struct thread* t = thread_current();
    
    if (t->cur_dir == NULL){
        t->cur_dir = dir_open_root();
        dir = dir_open_root();
    } else {
        dir = dir_reopen(t->cur_dir);    
    }

    if (dir != NULL)
        dir_lookup(dir, name, &inode);
    dir_close(dir);

    return file_open(inode);
}

/*! Deletes the file named NAME.  Returns true if successful, false on failure.
    Fails if no file named NAME exists, or if an internal memory allocation
    fails. */
bool filesys_remove(const char *name) {
    struct dir *dir;
    struct thread* t = thread_current();
    
    if (t->cur_dir == NULL){
        t->cur_dir = dir_open_root();
        dir = dir_open_root();
    } else {
        dir = dir_reopen(t->cur_dir);    
    }
    
    bool success = dir != NULL && dir_remove(dir, name);
    dir_close(dir);

    return success;
}

/*! Formats the file system. */
static void do_format(void) {
    printf("Formatting file system...");
    free_map_create();
    if (!dir_create(ROOT_DIR_SECTOR, 16, ROOT_DIR_SECTOR))
        PANIC("root directory creation failed");
    free_map_close();
    printf("done.\n");
}

