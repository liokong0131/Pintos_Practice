#ifndef FILESYS_DIRECTORY_H
#define FILESYS_DIRECTORY_H

#include <stdbool.h>
#include <stddef.h>
#include "devices/disk.h"
#include "filesys/symlink.h"
/* Maximum length of a file name component.
 * This is the traditional UNIX maximum length.
 * After directories are implemented, this maximum length may be
 * retained, but much longer full path names must be allowed. */
#define NAME_MAX 14
#define DEFAULT_ENTRY_CNT 16

struct inode;
struct symlink;
/* Opening and closing directories. */

struct dir *dir_open (struct inode *);
struct dir *dir_open_root (void);
struct dir *dir_reopen (struct dir *);
void dir_close (struct dir *);
bool dir_create (disk_sector_t sector, size_t entry_cnt, struct dir *parent_dir);
struct inode *dir_get_inode (struct dir *);

/* Reading and writing. */
bool dir_lookup (const struct dir *, const char *name, struct inode **);
bool dir_add (struct dir *, const char *name, disk_sector_t);
bool dir_remove (struct dir *, const char *name);
bool dir_readdir (struct dir *, char name[NAME_MAX + 1]);

// my implement function
bool dir_add_myself(struct dir *dir);
bool dir_add_parent(struct dir *dir, struct dir *parent_dir);
void directory_tokenize(char *dir, int *argc, char *argv[]);
bool change_directory(const char *path, int argc, char *argv[], struct dir *dir, struct dir **new_dirp, int ofs);
size_t dir_entry_count_used(struct dir *dir);
struct dir *dir_duplicate (struct dir *dir);
#endif /* filesys/directory.h */
