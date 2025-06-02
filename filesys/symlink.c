#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
struct symlink{
	struct inode *inode;        /* File's inode. */
};

struct symlink *
symlink_open (struct inode *inode) {
	struct symlink *symlink = calloc (1, sizeof *symlink);
	if (inode != NULL && symlink != NULL) {
		symlink->inode = inode;
		return symlink;
	} else {
		inode_close (inode);
		free (symlink);
		return NULL;
	}
}

struct symlink *
symlink_reopen (struct symlink *symlink) {
	return symlink_open (inode_reopen (symlink->inode));
}

void
symlink_close (struct symlink *symlink) {
	if (symlink != NULL) {
		inode_close (symlink->inode);
		free (symlink);
	}
}

bool
symlink_create (disk_sector_t sector, off_t initial_size, const char *target) {
	if(!inode_create (sector, initial_size, SYMLINK_INODE)){
        return false;
    }
	struct inode *inode = inode_open(sector);
    if(inode == NULL){
        return false;
    }
	bool success = inode_write_at(inode, target, initial_size, 0) == (strlen(target) + 1);
	inode_close(inode);
    return success;
}

struct inode *
symlink_get_inode (struct symlink *symlink) {
	return symlink->inode;
}

char *
symlink_set_path(struct symlink *symlink, struct dir *dir, struct dir **new_dirp){
	char *path = palloc_get_page(PAL_ZERO);
	if(path == NULL){
		return NULL;
	}
	if (inode_read_at(symlink->inode, path, PGSIZE, 0) <= 0) {
		palloc_free_page(path);
		return NULL;
	}
	
	char *path_copy = malloc(strlen(path) + 1);
	if (path_copy == NULL) {
		palloc_free_page(path);
		return NULL;
	}

	strlcpy(path_copy, path, strlen(path) + 1);
	
	struct dir *curDir = dir_reopen(dir);
	
	char *file_name = (char *)malloc(NAME_MAX + 1);
	if (file_name == NULL) {
		dir_close(curDir);
		palloc_free_page(path);
		free(path_copy);
		return NULL;
	}
	
	char *argv[32];
	int argc = 0;
	directory_tokenize(path_copy, &argc, argv);

	if(argc == 0){
		if(!strcmp("", path)){
			free(file_name);
			file_name = NULL;
			goto done;
		}
        dir_close(curDir);
		*new_dirp = dir_open_root();
		strlcpy(file_name, ".", (NAME_MAX + 1));
	} else{
		strlcpy(file_name, argv[argc-1], (NAME_MAX + 1));
		struct dir *new_dir = NULL;
		if(!change_directory(path, argc, argv, curDir, &new_dir, 1)){
            dir_close(curDir);
			free(file_name);
			file_name = NULL;
			goto done;
		}
		dir_close(curDir);
		*new_dirp = new_dir;
	}
	
done:
	palloc_free_page(path);
	free(path_copy);
	return file_name;
}

struct inode *
symlink_solve(struct symlink *symlink, struct dir *dir, struct dir **new_dirp){
	struct dir *curDir = dir_reopen(dir);
	struct inode *inode = NULL;
	//dir_entry_count_used(curDir);
	struct symlink *symlink_ = symlink_reopen(symlink);
	
	while(true){
		struct dir *new_dir = NULL;
		
		char *file_name = symlink_set_path(symlink_, curDir, &new_dir);
        symlink_close(symlink_);
		dir_close(curDir);
		if(file_name == NULL || new_dir == NULL) return NULL;
		curDir = new_dir;
		if(!dir_lookup(curDir, file_name, &inode)){
			dir_close(curDir);
            free(file_name);
			return NULL;
		}
        free(file_name);
		if(inode_get_type(inode) != SYMLINK_INODE) break;
		
		symlink_ = symlink_open(inode);
	}
    
    *new_dirp = curDir;
	return inode;
}