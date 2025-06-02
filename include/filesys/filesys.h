#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "filesys/inode.h"
/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0       /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /* Root directory file inode sector. */

struct open_info{
    void *obj;
    bool is_dir;
};

/* Disk used for file system. */
extern struct disk *filesys_disk;

void filesys_init (bool format);
void filesys_done (void);
bool filesys_create (const char *name, off_t initial_size, enum inode_status f_d_s, void *aux);
struct open_info *filesys_open (const char *name);
bool filesys_remove (const char *name);

//my implement functions
bool is_valid_file_name(const char *fn);
#endif /* filesys/filesys.h */
