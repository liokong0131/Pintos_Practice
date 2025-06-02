#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/fat.h"
#include "lib/kernel/hash.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* On-disk inode.
 * Must be exactly DISK_SECTOR_SIZE bytes long. */
struct inode_disk {
	disk_sector_t start;                /* First data sector. */
	cluster_t clst;
	cluster_t last_clst;
	off_t length;                       /* File size in bytes. */
	int f_d_s;
	unsigned magic;                     /* Magic number. */
	uint32_t unused[122];               /* Not used. */
};

/* Returns the number of sectors to allocate for an inode SIZE
 * bytes long. */
static inline size_t
bytes_to_sectors (off_t size) {
	return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode {
	struct list_elem elem;              /* Element in inode list. */
	disk_sector_t sector;               /* Sector number of disk location. */
	int open_cnt;                       /* Number of openers. */
	bool removed;                       /* True if deleted, false otherwise. */
	int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
	struct inode_disk data;             /* Inode content. */
};

/* Returns the disk sector that contains byte offset POS within
 * INODE.
 * Returns -1 if INODE does not contain data for a byte at offset
 * POS. */
static disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos, bool do_alloc) {
	ASSERT (inode != NULL);
	if (pos < inode->data.length){
		cluster_t clst = inode->data.clst;
		size_t target = pos / DISK_SECTOR_SIZE;
		cluster_t pclst = inode->data.clst;
		for(int i=0; i<pos / DISK_SECTOR_SIZE; i++){
			if(fat_info_get(clst) == target){
				return cluster_to_sector(clst);
			} else if( fat_info_get(clst) > target){
				
				if(!do_alloc){
					return -2;
				} else{
					return inode_fill_lazy_clst(pclst, target);
				}
			}
			pclst = clst;
			clst = fat_get(clst);
			if(clst == EOChain){
				return -1;
			}
		}
		return cluster_to_sector(clst);
	}
	return -1;
}

/* List of open inodes, so that opening a single inode twice
 * returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) {
	list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
 * writes the new inode to sector SECTOR on the file system
 * disk.
 * Returns true if successful.
 * Returns false if memory or disk allocation fails. */
bool
inode_create (disk_sector_t sector, off_t length, enum inode_status f_d_s) {
	struct inode_disk *disk_inode = NULL;
	bool success = false;
	ASSERT (length >= 0);

	/* If this assertion fails, the inode structure is not exactly
	 * one sector in size, and you should fix that. */
	ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

	disk_inode = calloc (1, sizeof *disk_inode);
	if (disk_inode != NULL) {
		cluster_t clst = fat_create_chain(0, 0);
		size_t sectors = bytes_to_sectors (length);
		disk_inode->length = length;
		disk_inode->f_d_s = f_d_s;
		disk_inode->magic = INODE_MAGIC;
		if (clst) {
			disk_inode->clst = clst;
			disk_inode->last_clst = clst;
			disk_inode->start = cluster_to_sector(clst);
			
			if (sectors > 0) {
				static char zeros[DISK_SECTOR_SIZE];
				size_t i;
				for (i = 0; i < sectors-1; i++) {
					disk_write (filesys_disk, cluster_to_sector(clst), zeros);
					clst = fat_create_chain(clst, i+1);
					if (clst == 0) return false;
				}
				disk_inode->last_clst = clst;
				disk_inode->f_d_s = f_d_s;
				disk_write (filesys_disk, cluster_to_sector(clst), zeros);
			}
			success = true; 
		} 
		disk_write (filesys_disk, sector, disk_inode);
		free (disk_inode);
	}
	return success;
}

/* Reads an inode from SECTOR
 * and returns a `struct inode' that contains it.
 * Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) {
	struct list_elem *e;
	struct inode *inode;

	/* Check whether this inode is already open. */
	for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
			e = list_next (e)) {
		inode = list_entry (e, struct inode, elem);
		if (inode->sector == sector) {
			inode_reopen (inode);
			return inode; 
		}
	}

	/* Allocate memory. */
	inode = malloc (sizeof *inode);
	if (inode == NULL)
		return NULL;

	/* Initialize. */
	list_push_front (&open_inodes, &inode->elem);
	inode->sector = sector;
	inode->open_cnt = 1;
	inode->deny_write_cnt = 0;
	inode->removed = false;
	disk_read (filesys_disk, inode->sector, &inode->data);
	return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode) {
	if (inode != NULL)
		inode->open_cnt++;
	return inode;
}

/* Returns INODE's inode number. */
disk_sector_t
inode_get_inumber (const struct inode *inode) {
	return inode->sector;
}

/* Closes INODE and writes it to disk.
 * If this was the last reference to INODE, frees its memory.
 * If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) {
	/* Ignore null pointer. */
	if (inode == NULL)
		return;
	
	/* Release resources if this was the last opener. */
	if (--inode->open_cnt == 0) {
		/* Remove from inode list and release lock. */
		list_remove (&inode->elem);
		struct disk *filesys_disk = disk_get (0, 1);
		disk_write(filesys_disk, inode->sector, &inode->data);
		/* Deallocate blocks if removed. */
		if (inode->removed) {
			fat_remove_chain(sector_to_cluster(inode->sector), 0);
			fat_remove_chain(inode->data.clst, 0);
		}
		free (inode); 
	}
}

/* Marks INODE to be deleted when it is closed by the last caller who
 * has it open. */
void
inode_remove (struct inode *inode) {
	ASSERT (inode != NULL);
	inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
 * Returns the number of bytes actually read, which may be less
 * than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) {
	uint8_t *buffer = buffer_;
	off_t bytes_read = 0;
	uint8_t *bounce = NULL;
	while (size > 0) {
		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually copy out of this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;
		
		/* Disk sector to read, starting byte offset within sector. */
		disk_sector_t sector_idx = byte_to_sector (inode, offset, false);
		if(sector_idx == (disk_sector_t)-1){
			free(bounce);
			return 0;
		}

		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) {
			/* Read full sector directly into caller's buffer. */
			if(sector_idx == (disk_sector_t)-2){
				memset(buffer + bytes_read, 0, DISK_SECTOR_SIZE);
			} else{
				disk_read (filesys_disk, sector_idx, buffer + bytes_read); 
			}
		} else {
			/* Read sector into bounce buffer, then partially copy
			 * into caller's buffer. */
			if(sector_idx == (disk_sector_t)-2){
				memcpy (buffer + bytes_read, 0, chunk_size);
			} else{
				if (bounce == NULL) {
					bounce = malloc (DISK_SECTOR_SIZE);
					if (bounce == NULL)
						break;
				}
				disk_read (filesys_disk, sector_idx, bounce);
				memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
			}
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_read += chunk_size;
	}
	free (bounce);

	return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
 * Returns the number of bytes actually written, which may be
 * less than SIZE if end of file is reached or an error occurs.
 * (Normally a write at end of file would extend the inode, but
 * growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
		off_t offset) {
	const uint8_t *buffer = buffer_;
	off_t bytes_written = 0;
	uint8_t *bounce = NULL;
	
	if (inode->deny_write_cnt){
		return 0;
	}

	// off_t new_size = offset + strlen(buffer) + 1 < offset + size ? offset + strlen(buffer) + 1 : offset + size;
	off_t new_size = offset + size;
	if(new_size > inode_length(inode)){
		inode_grow(inode, offset, new_size);
	}

	while (size > 0) {
		disk_sector_t sector_idx = byte_to_sector (inode, offset, true);
		if(sector_idx == (disk_sector_t)-1){
			free(bounce);
			return 0;
		} 
		
		/* Sector to write, starting byte offset within sector. */
		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;
		/* Number of bytes to actually write into this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) {
			/* Write full sector directly to disk. */
			disk_write (filesys_disk, sector_idx, buffer + bytes_written); 
		} else {
			/* We need a bounce buffer. */
			if (bounce == NULL) {
				bounce = malloc (DISK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}
			/* If the sector contains data before or after the chunk
			   we're writing, then we need to read in the sector
			   first.  Otherwise we start with a sector of all zeros. */
			if (sector_ofs > 0 || chunk_size < sector_left) 
				disk_read (filesys_disk, sector_idx, bounce);
			else
				memset (bounce, 0, DISK_SECTOR_SIZE);
			memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
			disk_write (filesys_disk, sector_idx, bounce); 
		}

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_written += chunk_size;
	}
	free (bounce);

	return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
	void
inode_deny_write (struct inode *inode) 
{
	inode->deny_write_cnt++;
	ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
 * Must be called once by each inode opener who has called
 * inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) {
	ASSERT (inode->deny_write_cnt > 0);
	ASSERT (inode->deny_write_cnt <= inode->open_cnt);
	inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode) {
	return inode->data.length;
}

// my implement function
enum inode_status
inode_get_type(struct inode *inode){
	return inode->data.f_d_s;
}

bool
is_inode_removed(struct inode *inode){
	return inode->removed;
}

void
inode_grow(struct inode *inode, off_t ofs, off_t new_size){
	size_t old_sector_idx = ofs > inode_length(inode) ? ofs / DISK_SECTOR_SIZE : inode_length(inode) / DISK_SECTOR_SIZE;
	size_t new_sector_idx = bytes_to_sectors(new_size);
	size_t alloc_sectors = new_sector_idx - old_sector_idx;
	if(old_sector_idx > fat_info_get(inode->data.last_clst)){
		alloc_sectors++;
	}

	cluster_t clst = inode->data.last_clst;
	for(int i=0; i<alloc_sectors; i++){
		clst = fat_create_chain(clst, old_sector_idx + i);
		if(clst == 0){
			return;
		}
	}
	inode->data.length = new_size;
	inode->data.last_clst = clst;

	disk_write(filesys_disk, inode->sector, &inode->data);
}

disk_sector_t
inode_fill_lazy_clst(uint32_t pclst, off_t file_idx){
	return cluster_to_sector(fat_insert_chain(pclst, file_idx));
}

void
inode_all_close(void){
	struct list_elem *iter = list_begin(&open_inodes);
	while(iter != list_end(&open_inodes)){
		struct list_elem *next = list_next(iter);
		struct inode *inode = list_entry(iter, struct inode, elem);
		inode->open_cnt = 1;
		inode_close(inode);
		iter = next;
	}
}