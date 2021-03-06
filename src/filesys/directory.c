#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"

/*! A directory. */
struct dir {
    struct inode *inode;                /*!< Backing store. */
    off_t pos;                          /*!< Current position. */
};

/*! A single directory entry. */
struct dir_entry {
    block_sector_t inode_sector;        /*!< Sector number of header. */
    char name[NAME_MAX + 1];            /*!< Null terminated file name. */
    bool in_use;                        /*!< In use or free? */
};

/*! Creates a directory with space for ENTRY_CNT entries in the
    given SECTOR and the parent dir's sector.  
    Returns true if successful, false on failure. */
bool dir_create(block_sector_t sector, size_t entry_cnt, 
                block_sector_t parent) {
    struct dir* dir;
    
    /* The directory name for parent directory*/
    const char par[3] = "..";
    
    /* The directory name for the current directory*/
    const char cur[2] = ".";
    
    /* Create a inode sector*/
    if (!inode_dir_create(sector, (entry_cnt + 2) * sizeof(struct dir_entry)))
        return false;
    
    /* Open the inode*/
    dir = dir_open(inode_open(sector));
    
    /* Add the current directory and the parent directory*/
    if (!dir_add(dir, cur, sector) || !dir_add(dir, par, parent)) {
        /* Otherwise remove the newly allocated inode */
        inode_remove(dir_get_inode(dir));
        /* Close this directory. */
        dir_close(dir);
        return false;    
    }
    
    return true;
        
}

/*! Opens and returns the directory for the given INODE, of which
    it takes ownership.  Returns a null pointer on failure. */
struct dir * dir_open(struct inode *inode) {
    struct dir *dir = calloc(1, sizeof(*dir));
    if (inode != NULL && dir != NULL && 
        inode->data.type == DIR_INODE_DISK) {
        
        dir->inode = inode;
        dir->pos = 0;
        return dir;
    }
    else {
        inode_close(inode);
        free(dir);
        return NULL; 
    }
}

/*! Opens the root directory and returns a directory for it.
    Return true if successful, false on failure. */
struct dir * dir_open_root(void) {
    return dir_open(inode_open(ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir * dir_reopen(struct dir *dir) {
    return dir_open(inode_reopen(dir->inode));
}

/*! Destroys DIR and frees associated resources. */
void dir_close(struct dir *dir) {
    if (dir != NULL) {
        inode_close(dir->inode);
        free(dir);
    }
}

/*! Returns the inode encapsulated by DIR. */
struct inode * dir_get_inode(struct dir *dir) {
    return dir->inode;
}

/*! Searches DIR for a file with the given NAME.
    If successful, returns true, sets *EP to the directory entry
    if EP is non-null, and sets *OFSP to the byte offset of the
    directory entry if OFSP is non-null.
    otherwise, returns false and ignores EP and OFSP. */
static bool lookup(const struct dir *dir, const char *name,
                   struct dir_entry *ep, off_t *ofsp) {
    struct dir_entry e;
    size_t ofs;

    ASSERT(dir != NULL);
    ASSERT(name != NULL);

    for (ofs = 0; inode_read_at(dir->inode, &e, sizeof(e), ofs) == sizeof(e);
         ofs += sizeof(e)) {
        if (e.in_use && !strcmp(name, e.name)) {
            if (ep != NULL)
                *ep = e;
            if (ofsp != NULL)
                *ofsp = ofs;
            return true;
        }
    }
    return false;
}

/*! Searches DIR for a file with the given NAME and returns true if one exist,
    false otherwise.  On success, sets *INODE to an inode for the file,
    otherwise to a null pointer.  The caller must close *INODE. */
bool dir_lookup(const struct dir *dir, const char *name, 
                struct inode **inode) {
    struct dir_entry e;

    ASSERT(dir != NULL);
    ASSERT(name != NULL);

    if (lookup(dir, name, &e, NULL))
        *inode = inode_open(e.inode_sector);
    else
        *inode = NULL;

    return *inode != NULL;
}

/*! Adds a file named NAME to DIR, which must not already contain a file by
    that name.  The file's inode is in sector INODE_SECTOR.
    Returns true if successful, false on failure.
    Fails if NAME is invalid (i.e. too long) or a disk or memory
    error occurs. */
bool dir_add(struct dir *dir, const char *name, block_sector_t inode_sector) {
    struct dir_entry e;
    off_t ofs;
    bool success = false;

    ASSERT(dir != NULL);
    ASSERT(name != NULL);

    /* Check NAME for validity. */
    if (*name == '\0' || strlen(name) > NAME_MAX)
        return false;

    /* Check that NAME is not in use. */
    if (lookup(dir, name, NULL, NULL))
        goto done;

    /* Set OFS to offset of free slot.
       If there are no free slots, then it will be set to the
       current end-of-file.
     
       inode_read_at() will only return a short read at end of file.
       Otherwise, we'd need to verify that we didn't get a short
       read due to something intermittent such as low memory. */
    for (ofs = 0; inode_read_at(dir->inode, &e, sizeof(e), ofs) == sizeof(e);
         ofs += sizeof(e)) {
        if (!e.in_use)
            break;
    }

    /* Write slot. */
    e.in_use = true;
    strlcpy(e.name, name, sizeof e.name);
    e.inode_sector = inode_sector;
    success = inode_write_at(dir->inode, &e, sizeof(e), ofs) == sizeof(e);

done:
    return success;
}

/*! Removes any entry for NAME in DIR.  Returns true if successful, false on
    failure, which occurs only if there is no file with the given NAME. */
bool dir_remove(struct dir *dir, const char *name) {
    struct dir_entry e, sub_e;
    struct inode *inode = NULL;
    bool success = false;
    off_t ofs;
    struct dir* subdir;
    const char tmp[NAME_MAX + 1];

    ASSERT(dir != NULL);
    ASSERT(name != NULL);

    /* Find directory entry. */
    if (!lookup(dir, name, &e, &ofs))
        goto done;

    /* Open inode. */
    inode = inode_open(e.inode_sector);
    if (inode == NULL)
        goto done;

    if (inode->data.type == FILE_INODE_DISK){
        /* If we are removing a file, then remove the entry right away. */
        
        /* Erase directory entry. */
        e.in_use = false;
        if (inode_write_at(dir->inode, &e, sizeof(e), ofs) != sizeof(e))
            goto done;

        /* Remove inode. */
        inode_remove(inode);
        success = true;
    } else if (inode->data.type == DIR_INODE_DISK){
        /* If we are actually removing a subdirectory, then we can only 
         * remove if the directory is empty. */
        
        /* Open up the sub-directory*/ 
        subdir = dir_open(inode);
        
        /* Check whether the sub-directory is empty. */
        if (dir_readdir(subdir, name)){
            /* If it is not empty, then free dir we just opened and return. */
            if (subdir != NULL)
                free(subdir);
             
            goto done;
        } else {
            /* We do not need this directory any more, then just free it. */
            free(subdir);

            /* Erase directory entry. */
            e.in_use = false;
            if (inode_write_at(dir->inode, &e, sizeof(e), ofs) != sizeof(e))
                goto done;

            /* Remove inode. */
            inode_remove(inode);
            success = true;
        }
    }
done:
    inode_close(inode);
    return success;
}

/*! Reads the next directory entry in DIR and stores the name in NAME. Returns
    true if successful, false if the directory contains no more entries. */
bool dir_readdir(struct dir *dir, char name[NAME_MAX + 1]) {
    struct dir_entry e;
    
    /* Parent directory name. */
    char par[3] = "..";
    /* Current diretory name. */
    char cur[2] = ".";

    while (inode_read_at(dir->inode, &e, sizeof(e), dir->pos) == sizeof(e)) {
        dir->pos += sizeof(e);
        /* return true if the entry is in use and the entry is neither the 
         * parent dir nor the current dir. */
        if (e.in_use && (strcmp(e.name, par) != 0) 
                     && (strcmp(e.name, cur) != 0)) {
            strlcpy(name, e.name, NAME_MAX + 1);
            return true;
        } 
    }
    return false;
}

