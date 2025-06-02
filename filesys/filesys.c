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
#include "threads/vaddr.h"
#include "filesys/symlink.h"

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
}

/* Shuts down the file system module, writing any unwritten data
 * to disk. */
void
filesys_done (void) {
	/* Original FS */
#ifdef EFILESYS
	inode_all_close();
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
filesys_create (const char *name, off_t initial_size, enum inode_status f_d_s, void *aux) {
	char *name_copy = palloc_get_page(PAL_ZERO);
	if(name_copy == NULL){
		return false;
	}
	strlcpy(name_copy, name, PGSIZE);

	struct dir *curDir = (thread_current()->cwd == NULL) ? dir_open_root() : dir_reopen(thread_current()->cwd);
	int argc = 0;
	char *argv[32];
	char *file_name;
	bool success = false;
	directory_tokenize(name_copy, &argc, argv);
	if(argc == 0 || !is_valid_file_name(argv[argc-1])){
		goto done;
	}
	file_name = argv[argc-1];
	struct dir *new_dir = NULL;
	if(!change_directory(name, argc, argv, curDir, &new_dir, 1)){
		dir_close(new_dir);
		goto done;
	}
	dir_close(curDir);
	curDir = new_dir;
	if(is_inode_removed(dir_get_inode(curDir))){
		goto done;
	}
	
	cluster_t clst;
	clst = fat_create_chain(0, 0);
	disk_sector_t inode_sector = cluster_to_sector(clst);
	success = curDir != NULL && clst != 0;
	if (!success){
		fat_remove_chain(clst, 0);
		goto done;
	}
	switch (f_d_s) {
	case FILE_INODE:
		success = success && file_create (inode_sector, initial_size);
		break;
	case DIR_INODE:
		success = success && dir_create(inode_sector, initial_size, curDir);
		break;
	case SYMLINK_INODE:
		success = success && symlink_create(inode_sector, initial_size, aux);
		break;
	default:
		success = false;
		goto done;
	}
    success = success && dir_add (curDir, file_name, inode_sector);
	
	if (!success)
		fat_remove_chain(clst, 0);
	
done:
	palloc_free_page(name_copy);
	dir_close (curDir);
	return success;
}

/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
struct open_info *
filesys_open (const char *name) {
	char *name_copy = palloc_get_page(PAL_ZERO);
	if(name_copy == NULL){
		return NULL;
	}
	strlcpy(name_copy, name, PGSIZE);
	
	struct open_info *o_info;
	struct inode *inode = NULL;
	struct dir *curDir = (thread_current()->cwd == NULL) ? dir_open_root() : dir_reopen(thread_current()->cwd);
	int argc = 0;
	char *file_name = NULL;
	char *argv[32];
	directory_tokenize(name_copy, &argc, argv);
	if(argc == 0){
		if(!strcmp("", name)) goto error;
		inode = dir_get_inode(dir_open_root());
	} else{
		file_name = argv[argc-1];
		struct dir *new_dir = NULL;
		if(!change_directory(name, argc, argv, curDir, &new_dir, 1)){
			dir_close(new_dir);
			goto error;
		}
		dir_close(curDir);
		curDir = new_dir;
		if(is_inode_removed(dir_get_inode(curDir))) goto error;
		if (curDir != NULL){
			if(!dir_lookup (curDir, file_name, &inode)){
				goto error;
			}
		}
	}
	palloc_free_page(name_copy);

	o_info = (struct open_info *)malloc(sizeof(struct open_info));
	if(o_info == NULL){
		dir_close (curDir);
		return NULL;
	}	

	switch(inode_get_type(inode)){
	case FILE_INODE:
		o_info->is_dir = false;
		o_info->obj = file_open (inode);
		break;
	case DIR_INODE:
		o_info->is_dir = true;
		o_info->obj = dir_open(inode);
		break;
	case SYMLINK_INODE:{
		struct symlink *symlink = symlink_open(inode);
		if(symlink == NULL){
			free(o_info);
			dir_close (curDir);
			return NULL;
		}

		struct dir *new_dir = NULL;
		struct inode *ninode = symlink_solve(symlink, curDir, &new_dir);
		if(ninode == NULL){
			symlink_close(symlink);
			free(o_info);
			dir_close (curDir);
			dir_close (new_dir);
			return NULL;
		}
		
		dir_close(curDir);
		curDir = new_dir;
		switch (inode_get_type(ninode)){
		case FILE_INODE:
			o_info->is_dir = false;
			o_info->obj = file_open (ninode);
			break;
		case DIR_INODE:
			o_info->is_dir = true;
			o_info->obj = dir_open(ninode);
			break;
		default:
			symlink_close(symlink);
			free(o_info);
			dir_close (curDir);
			return NULL;
		}
		symlink_close(symlink);
		break;
	}default:
		
		free(o_info);
		dir_close (curDir);
		return NULL;
	}
	dir_close (curDir);
	return o_info;

error:
	palloc_free_page(name_copy);
	dir_close(curDir);
	return NULL;
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) {
	char *name_copy = palloc_get_page(PAL_ZERO);
	if(name_copy == NULL){
		return false;
	}
	strlcpy(name_copy, name, PGSIZE);

	struct dir *curDir = (thread_current()->cwd == NULL) ? dir_open_root() : dir_reopen(thread_current()->cwd);
	int argc = 0;
	char *argv[32];
	char *file_name;
	bool success = false;
	directory_tokenize(name_copy, &argc, argv);
	if(argc == 0){
		goto done;
	}
	file_name = argv[argc-1];
	struct dir *new_dir = NULL;
	if(!change_directory(name, argc, argv, curDir, &new_dir, 1)){
		dir_close(new_dir);
		goto done;
	}
	dir_close(curDir);
	curDir = new_dir;
	if(is_inode_removed(dir_get_inode(curDir))) return false;
	if(!is_valid_file_name(file_name)) return false;

	success = (curDir != NULL) && dir_remove (curDir, file_name);

done:
	palloc_free_page(name_copy);
	dir_close (curDir);
	return success;
}

/* Formats the file system. */
static void
do_format (void) {
	printf ("Formatting file system...");

#ifdef EFILESYS
	/* Create FAT and save it to the disk. */
	fat_create ();
	if(!dir_create (cluster_to_sector(ROOT_DIR_CLUSTER), DEFAULT_ENTRY_CNT, NULL)){
		PANIC ("root directory creation failed");
	}
	fat_close ();
#else
	free_map_create ();
	if (!dir_create (ROOT_DIR_SECTOR, 16, NULL))
		PANIC ("root directory creation failed");
	free_map_close ();
#endif

	printf ("done.\n");
}

// my implement functions
bool
is_valid_file_name(const char *fn){
	if(fn == NULL || !strcmp(fn, "") || !strcmp(fn, ".")
		|| !strcmp(fn, "..") || strlen(fn) > NAME_MAX){
		return false;
	}
	return true;
}