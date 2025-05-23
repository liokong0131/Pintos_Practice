/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/mmu.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

struct bitmap *swap_table;

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);

	size_t swap_page_size = disk_size(swap_disk) / 8;
	
	swap_table = bitmap_create(swap_page_size);
	ASSERT(swap_table != NULL);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	/* page initialize */
	struct anon_page *anon_page = &page->anon;
	page->is_in_mem = true;

	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
	lock_acquire(thread_current()->swap_lock);
	for(int i=0; i<8; i++){
		disk_read(swap_disk, (anon_page->swap_sector * 8) + i, kva + (i * DISK_SECTOR_SIZE));
	}
	bitmap_set(swap_table, anon_page->swap_sector, false);
	lock_release(thread_current()->swap_lock);
	page->is_in_mem = true;
	return true;
}
	

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	lock_acquire(thread_current()->swap_lock);
	anon_page->swap_sector = bitmap_scan_and_flip(swap_table, 0, 1, false);
	if(anon_page->swap_sector == BITMAP_ERROR){
		lock_release(thread_current()->swap_lock);
		return false;
	}
	for(int i=0; i<8; i++){
		disk_write(swap_disk, (anon_page->swap_sector * 8) + i, page->frame->kva + (i * DISK_SECTOR_SIZE));
	}
	lock_release(thread_current()->swap_lock);
	page->is_in_mem = false;
	page->frame = NULL;
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	if(!page->is_in_mem){
		lock_acquire(thread_current()->swap_lock);
		bitmap_set(swap_table, anon_page->swap_sector, false);
		lock_release(thread_current()->swap_lock);
	}else{
		lock_acquire(thread_current()->swap_lock);
		list_remove(&page->frame->l_elem);
		lock_release(thread_current()->swap_lock);
		free(page->frame);
	}
	if(page->f_info != NULL)
		free(page->f_info);
	return;
}

bool
anon_page_copy (struct page *page, void *aux){
	struct copy_info *c_info = (struct copy_info *)aux;
	struct page *parent_page = c_info->parent_page;

	if(parent_page->f_info != NULL){
		page->f_info = (struct file_info *)malloc(sizeof(struct file_info));
		page->f_info->file = parent_page->f_info->file;
		page->f_info->offset = parent_page->f_info->offset;
		page->f_info->read_bytes = parent_page->f_info->read_bytes;
		page->f_info->zero_bytes = parent_page->f_info->zero_bytes;
	}

	if(parent_page->is_in_mem){
		struct frame *child_frame = page->frame;
		memcpy(child_frame->kva, parent_page->frame->kva, PGSIZE);
		page->is_in_mem = true;
	}else{
		struct anon_page *anon_parent_page = &parent_page->anon;
		struct anon_page *anon_child_page = &page->anon;
		size_t cache_idx = 0;
		struct disk *swap_disk = disk_get(1, 1);
		void *buffer = malloc(DISK_SECTOR_SIZE);
		if(buffer == NULL){
			return false;
		}
		lock_acquire(thread_current()->swap_lock);
		anon_child_page->swap_sector = bitmap_scan_and_flip(swap_table, 0, 1, false);
		for(int i=0; i<8; i++){
			disk_read(swap_disk, (anon_parent_page->swap_sector * 8) + i, buffer);
			disk_write(swap_disk, (anon_child_page->swap_sector * 8) + i, buffer);
		}
		lock_release(thread_current()->swap_lock);
		free(buffer);
		page->is_in_mem = false;
	}
	
	return true;
}