/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/malloc.h"
#include "threads/synch.h"

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
struct lock swap_lock;

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);

	size_t swap_page_size = disk_size(swap_disk) / 8;
	
	swap_table = bitmap_create(swap_page_size);
	ASSERT(swap_table != NULL);

	bitmap_set_all(swap_table, false);
	
	lock_init(&swap_lock);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	/* page initialize */
	struct anon_page *anon_page = &page->anon;
	anon_page->is_in_mem = true;
	anon_page->swap_table = swap_table;
	anon_page->swap_lock = &swap_lock;

	struct frame *frame = malloc(sizeof(struct frame));
	frame->kva = kva;
	frame->page = page;
	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;

	lock_acquire(&swap_lock);

	for(int i=0; i<8; i++){
		disk_read(swap_disk, anon_page->swap_sectors[i], kva + (i * DISK_SECTOR_SIZE));
	}

	struct frame *frame = malloc(sizeof(struct frame));
	if (frame == NULL) {
		PANIC("(in anon swap_in frame malloc) Fail");
	}

	frame->kva = kva;
	frame->page = page;

	page->frame = frame;
	anon_page->is_in_mem = true;
}
	

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	size_t cache_idx = 0;
	for(int i=0; i<8; i++){
		anon_page->swap_sectors[i] = bitmap_scan_and_flip(swap_table, cache_idx, 1, true);
    	if(anon_page->swap_sectors[i] == BITMAP_ERROR){
			PANIC("(in anon) Swap allocation failed: swap_sectors index %d", i);
		}

		cache_idx = anon_page->swap_sectors[i];
		disk_write(swap_disk, cache_idx, page->frame->kva + (i * DISK_SECTOR_SIZE));
	}

	palloc_free_page(page->frame->kva);
	free(page->frame);
	page->frame = NULL;
	anon_page->is_in_mem = false;
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;

	if(anon_page->is_in_mem){
		palloc_free_page(page->frame->kva);
		free(page->frame);
	} else{
		for(int i=0; i<8; i++){
			bitmap_set(swap_table, anon_page->swap_sectors[i], false);
		}
	}
}
