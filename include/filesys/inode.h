#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/disk.h"
#include "filesys/fat.h"
struct bitmap;

enum inode_status {
	FILE_INODE,
	DIR_INODE,
	SYMLINK_INODE,
};

void inode_init (void);
bool inode_create (disk_sector_t, off_t, enum inode_status f_d_s);
struct inode *inode_open (disk_sector_t);
struct inode *inode_reopen (struct inode *);
disk_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

// my implement function
enum inode_status inode_get_type(struct inode *inode);
bool is_inode_removed(struct inode *inode);
void inode_grow(struct inode *inode, off_t ofs, off_t new_size);
disk_sector_t inode_fill_lazy_clst(uint32_t pclst, off_t file_idx);
void inode_all_close(void);
#endif /* filesys/inode.h */
