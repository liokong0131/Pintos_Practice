#ifndef FILESYS_SYMLINK_H
#define FILESYS_SYMLINK_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "filesys/inode.h"
#include "filesys/directory.h"

struct inode;
struct dir;

struct symlink *symlink_open (struct inode *inode);
struct symlink *symlink_reopen (struct symlink *symlink);
void symlink_close (struct symlink *symlink);
bool symlink_create (disk_sector_t sector, off_t initial_size, const char *target);
struct inode *symlink_get_inode (struct symlink *symlink);
char *symlink_set_path(struct symlink *symlink, struct dir *dir, struct dir **new_dirp);
struct inode *symlink_solve(struct symlink *symlink, struct dir *dir, struct dir **new_dirp);

#endif