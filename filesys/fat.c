#include "filesys/fat.h"
#include "devices/disk.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include <stdio.h>
#include <string.h>

/* Should be less than DISK_SECTOR_SIZE */
struct fat_boot {
	unsigned int magic;
	unsigned int sectors_per_cluster; /* Fixed to 1 */
	unsigned int total_sectors;
	unsigned int fat_start;
	unsigned int fat_info_start;
	unsigned int fat_sectors; /* Size of FAT in sectors. */
	unsigned int root_dir_cluster;
	unsigned int fat_last_clst;
};

/* FAT FS */
struct fat_fs {
	struct fat_boot bs;
	unsigned int *fat;
	unsigned int *fat_info;
	unsigned int fat_length;
	disk_sector_t data_start;
	cluster_t last_clst;
	struct lock write_lock;
};

static struct fat_fs *fat_fs;

void fat_boot_create (void);
void fat_fs_init (void);
cluster_t find_empty_cluster(void);

void
fat_init (void) {
	fat_fs = calloc (1, sizeof (struct fat_fs));
	if (fat_fs == NULL)
		PANIC ("FAT init failed");

	// Read boot sector from the disk
	unsigned int *bounce = malloc (DISK_SECTOR_SIZE);
	if (bounce == NULL)
		PANIC ("FAT init failed");
	disk_read (filesys_disk, FAT_BOOT_SECTOR, bounce);
	memcpy (&fat_fs->bs, bounce, sizeof (fat_fs->bs));
	free (bounce);

	// Extract FAT info
	if (fat_fs->bs.magic != FAT_MAGIC)
		fat_boot_create ();
	fat_fs_init ();
}

void
fat_open (void) {
	fat_fs->fat = calloc (fat_fs->fat_length, sizeof (cluster_t));
	if (fat_fs->fat == NULL)
		PANIC ("FAT load failed");
	fat_fs->fat_info = calloc (fat_fs->fat_length, sizeof (cluster_t));
	if (fat_fs->fat_info == NULL)
		PANIC ("FAT_INFO load failed");

	// Load FAT directly from the disk
	uint8_t *buffer = (uint8_t *) fat_fs->fat;
	off_t bytes_read = 0;
	off_t bytes_left = sizeof (fat_fs->fat);
	const off_t fat_size_in_bytes = fat_fs->fat_length * sizeof (cluster_t);
	for (unsigned i = 0; i < fat_fs->bs.fat_sectors; i++) {
		bytes_left = fat_size_in_bytes - bytes_read;
		if (bytes_left >= DISK_SECTOR_SIZE) {
			disk_read (filesys_disk, fat_fs->bs.fat_start + i,
			           buffer + bytes_read);
			bytes_read += DISK_SECTOR_SIZE;
		} else {
			uint8_t *bounce = malloc (DISK_SECTOR_SIZE);
			if (bounce == NULL)
				PANIC ("FAT load failed");
			disk_read (filesys_disk, fat_fs->bs.fat_start + i, bounce);
			memcpy (buffer + bytes_read, bounce, bytes_left);
			bytes_read += bytes_left;
			free (bounce);
		}
	}

	// Load FAT_INFO directly from the disk
	buffer = (uint8_t *) fat_fs->fat_info;
	bytes_read = 0;
	bytes_left = sizeof (fat_fs->fat_info);
	for (unsigned i = 0; i < fat_fs->bs.fat_sectors; i++) {
		bytes_left = fat_size_in_bytes - bytes_read;
		if (bytes_left >= DISK_SECTOR_SIZE) {
			disk_read (filesys_disk, fat_fs->bs.fat_info_start + i,
			           buffer + bytes_read);
			bytes_read += DISK_SECTOR_SIZE;
		} else {
			uint8_t *bounce = malloc (DISK_SECTOR_SIZE);
			if (bounce == NULL)
				PANIC ("FAT load failed");
			disk_read (filesys_disk, fat_fs->bs.fat_info_start + i, bounce);
			memcpy (buffer + bytes_read, bounce, bytes_left);
			bytes_read += bytes_left;
			free (bounce);
		}
	}
}

void
fat_close (void) {
	// Write FAT boot sector
	fat_fs->bs.fat_last_clst = fat_fs->last_clst;
	uint8_t *bounce = calloc (1, DISK_SECTOR_SIZE);
	if (bounce == NULL)
		PANIC ("FAT close failed");
	memcpy (bounce, &fat_fs->bs, sizeof (fat_fs->bs));
	disk_write (filesys_disk, FAT_BOOT_SECTOR, bounce);
	free (bounce);

	// Write FAT directly to the disk
	uint8_t *buffer = (uint8_t *) fat_fs->fat;
	off_t bytes_wrote = 0;
	off_t bytes_left = sizeof (fat_fs->fat);
	const off_t fat_size_in_bytes = fat_fs->fat_length * sizeof (cluster_t);
	for (unsigned i = 0; i < fat_fs->bs.fat_sectors; i++) {
		bytes_left = fat_size_in_bytes - bytes_wrote;
		if (bytes_left >= DISK_SECTOR_SIZE) {
			disk_write (filesys_disk, fat_fs->bs.fat_start + i,
			            buffer + bytes_wrote);
			bytes_wrote += DISK_SECTOR_SIZE;
		} else {
			bounce = calloc (1, DISK_SECTOR_SIZE);
			if (bounce == NULL)
				PANIC ("FAT close failed");
			memcpy (bounce, buffer + bytes_wrote, bytes_left);
			disk_write (filesys_disk, fat_fs->bs.fat_start + i, bounce);
			bytes_wrote += bytes_left;
			free (bounce);
		}
	}

	// Write FAT directly to the disk
	buffer = (uint8_t *) fat_fs->fat_info;
	bytes_wrote = 0;
	bytes_left = sizeof (fat_fs->fat_info);
	for (unsigned i = 0; i < fat_fs->bs.fat_sectors; i++) {
		bytes_left = fat_size_in_bytes - bytes_wrote;
		if (bytes_left >= DISK_SECTOR_SIZE) {
			disk_write (filesys_disk, fat_fs->bs.fat_info_start + i,
			            buffer + bytes_wrote);
			bytes_wrote += DISK_SECTOR_SIZE;
		} else {
			bounce = calloc (1, DISK_SECTOR_SIZE);
			if (bounce == NULL)
				PANIC ("FAT close failed");
			memcpy (bounce, buffer + bytes_wrote, bytes_left);
			disk_write (filesys_disk, fat_fs->bs.fat_info_start + i, bounce);
			bytes_wrote += bytes_left;
			free (bounce);
		}
	}

	// free (fat_fs->fat);
	// free (fat_fs->fat_info);
  	//free (fat_fs);
}

void
fat_create (void) {
	// Create FAT boot
	fat_boot_create ();
	fat_fs_init ();

	// Create FAT table
	fat_fs->fat = calloc (fat_fs->fat_length, sizeof (cluster_t));
	if (fat_fs->fat == NULL)
		PANIC ("FAT creation failed");
	fat_fs->fat_info = calloc (fat_fs->fat_length, sizeof (cluster_t));
	if (fat_fs->fat_info == NULL)
		PANIC ("FAT creation failed");

	// Set up ROOT_DIR_CLST
	fat_put (ROOT_DIR_CLUSTER, EOChain);
	fat_info_put(ROOT_DIR_CLUSTER, 0);
	
	// Fill up ROOT_DIR_CLUSTER region with 0
	uint8_t *buf = calloc (1, DISK_SECTOR_SIZE);
	if (buf == NULL)
		PANIC ("FAT create failed due to OOM");
	
	disk_write (filesys_disk, cluster_to_sector (ROOT_DIR_CLUSTER), buf);
	free (buf);
}

void
fat_boot_create (void) {
	unsigned int fat_sectors =
	    (disk_size (filesys_disk) - 1)
	    / (DISK_SECTOR_SIZE / sizeof (cluster_t) * SECTORS_PER_CLUSTER + 1) + 1;
	fat_fs->bs = (struct fat_boot){
	    .magic = FAT_MAGIC,
	    .sectors_per_cluster = SECTORS_PER_CLUSTER,
	    .total_sectors = disk_size (filesys_disk),
	    .fat_start = 1,
		.fat_info_start = 1 + fat_sectors,
	    .fat_sectors = fat_sectors,
	    .root_dir_cluster = ROOT_DIR_CLUSTER,
		.fat_last_clst = ROOT_DIR_CLUSTER + 1,
	};
}

void
fat_fs_init (void) {
	/* TODO: Your code goes here. */
	struct fat_boot *bs = &fat_fs->bs;
	fat_fs->fat_length = bs->fat_sectors * (DISK_SECTOR_SIZE / sizeof (cluster_t));
	fat_fs->data_start = bs->fat_info_start + bs->fat_sectors;
	fat_fs->last_clst = bs->fat_last_clst;
	lock_init(&fat_fs->write_lock);
}

/*----------------------------------------------------------------------------*/
/* FAT handling                                                               */
/*----------------------------------------------------------------------------*/

/* Add a cluster to the chain.
 * If CLST is 0, start a new chain.
 * Returns 0 if fails to allocate a new cluster. */
cluster_t
fat_create_chain (cluster_t clst, uint32_t file_idx) {
	/* TODO: Your code goes here. */
	cluster_t result = 0;

	lock_acquire(&fat_fs->write_lock);
	if(clst != 0){
		fat_put(clst, fat_fs->last_clst);
	}
	fat_put(fat_fs->last_clst, EOChain);
	fat_info_put(fat_fs->last_clst, file_idx);
	result = fat_fs->last_clst;
	
	fat_fs->last_clst = find_empty_cluster();

	lock_release(&fat_fs->write_lock);
	return result;
}

/* Remove the chain of clusters starting from CLST.
 * If PCLST is 0, assume CLST as the start of the chain. */
void
fat_remove_chain (cluster_t clst, cluster_t pclst) {
	/* TODO: Your code goes here. */
	if(clst == 0 || fat_get(clst) == 0){
		return;
	}

	cluster_t clst_ = clst;
	cluster_t nclst_;
	cluster_t minClst = fat_fs->last_clst;

	lock_acquire(&fat_fs->write_lock);
	if(pclst != 0){
		fat_put(pclst, EOChain);
	}

	while(clst_ != EOChain){
		nclst_ = fat_get(clst_);
		fat_put(clst_, 0);
		fat_info_put(clst_, 0);
		if(clst_ < minClst){
			minClst = clst_;
		}
		clst_ = nclst_;
	}

	fat_fs->last_clst = minClst;
	lock_release(&fat_fs->write_lock);
}

/* Update a value in the FAT table. */
void
fat_put (cluster_t clst, cluster_t val) {
	/* TODO: Your code goes here. */
	fat_fs->fat[clst] = val;
}

/* Fetch a value in the FAT table. */
cluster_t
fat_get (cluster_t clst) {
	/* TODO: Your code goes here. */
	return fat_fs->fat[clst];
}

/* Covert a cluster # to a sector number. */
disk_sector_t
cluster_to_sector (cluster_t clst) {
	/* TODO: Your code goes here. */
	return fat_fs->data_start + (clst - ROOT_DIR_CLUSTER);
}


// my implement funcions
cluster_t
find_empty_cluster(void){
	cluster_t idx = fat_fs->last_clst;

	while(idx < fat_fs->fat_length){
		if(fat_fs->fat[idx] == 0){
			return idx;
		}
		idx++;
	}
	return 0;
}

void
fat_info_put (cluster_t clst, uint32_t val) {
	/* TODO: Your code goes here. */
	fat_fs->fat_info[clst] = val;
}

uint32_t
fat_info_get (cluster_t clst) {
	/* TODO: Your code goes here. */
	return fat_fs->fat_info[clst];
}

cluster_t
fat_insert_chain(cluster_t clst, uint32_t file_idx){
	cluster_t end_clst = fat_get(clst);
	cluster_t insert_clst = fat_create_chain(clst, file_idx);
	fat_put(insert_clst, end_clst);
	return insert_clst;
}

cluster_t
sector_to_cluster (disk_sector_t sector) {
	/* TODO: Your code goes here. */
	return sector - fat_fs->data_start + ROOT_DIR_CLUSTER;
}