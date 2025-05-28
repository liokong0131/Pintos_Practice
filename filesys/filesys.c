#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "filesys/fat.h"
#include "threads/thread.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) {
	filesys_disk = disk_get (0, 1);
	if (filesys_disk == NULL)
		PANIC ("hd0:1 (hdb) not present, file system initialization failed");

	inode_init ();
#ifdef EFILESYS
	fat_init ();

	if (format)
		do_format ();
	fat_open ();
#else
	/* Original FS */
	free_map_init ();

	if (format)
		do_format ();

	free_map_open ();
#endif
	if (format && !dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
}

/* Shuts down the file system module, writing any unwritten data
 * to disk. */
void
filesys_done (void) {
	/* Original FS */
#ifdef EFILESYS
	fat_close ();
#else
	free_map_close ();
#endif
}

/* Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) {
	disk_sector_t inode_sector = 0;
	
	struct dir *curDir = (thread_current()->cwd == NULL) ? dir_open_root() : dir_reopen(thread_current()->cwd);
	int argc = 0;
	char *argv[128];
	char *file_name;
	directory_tokenize(name, &argc, argv);
	if(argc == 0){
		dir_close(curDir);
		return false;
	}
	file_name = argv[argc-1];
	change_directory(name, argc, argv, &curDir, 1);
	
	cluster_t clst;
	clst = fat_create_chain(0);
	inode_sector = cluster_to_sector(clst);
	bool success = (curDir != NULL
				&& clst != 0
				&& inode_create (inode_sector, initial_size)
				&& dir_add (curDir, file_name, inode_sector));
	
	if (!success)
		fat_remove_chain(clst, 0);

	dir_close (curDir);
	return success;
}

/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name) {
	struct inode *inode = NULL;
	struct dir *curDir = (thread_current()->cwd == NULL) ? dir_open_root() : dir_reopen(thread_current()->cwd);
	int argc = 0;
	char *argv[128];
	char *file_name;
	directory_tokenize(name, &argc, argv);
	if(argc == 0){
		dir_close(curDir);
		return false;
	}
	file_name = argv[argc-1];
	change_directory(name, argc, argv, &curDir, 1);
	
	if (curDir != NULL)
		dir_lookup (curDir, file_name, &inode);
	dir_close (curDir);
	
	return file_open (inode);
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) {
	struct dir *dir = dir_open_root ();
	bool success = dir != NULL && dir_remove (dir, name);
	dir_close (dir);

	return success;
}

/* Formats the file system. */
static void
do_format (void) {
	printf ("Formatting file system...");

#ifdef EFILESYS
	/* Create FAT and save it to the disk. */
	fat_create ();
	fat_close ();
#else
	free_map_create ();
	if (!dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
	free_map_close ();
#endif

	printf ("done.\n");
}
