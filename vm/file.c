/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

static struct disk *file_disk;

/* The initializer of file vm */
void
vm_file_init (void) {
	file_disk = disk_get(0, 1);
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	// struct file_info *f_info = page->aux;
	// file_page->f_info.file = f_info->file;
	// file_page->f_info.offset = f_info->offset;
	// file_page->f_info.read_bytes = f_info->read_bytes;
	// file_page->f_info.zero_bytes = f_info->zero_bytes;

	// free(f_info);

	// file_page->is_in_mem = true;

	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page = &page->file;
	struct file_info *f_info = &file_page->f_info;
	off_t bytes_read = file_read_at(f_info->file, kva, f_info->read_bytes, f_info->offset);
	if(bytes_read != (off_t)f_info->read_bytes){
		return false;
	}

	memset(kva + f_info->read_bytes, 0, f_info->zero_bytes);
	file_page->is_in_mem = true;

	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page = &page->file;
	struct file_info *f_info = &file_page->f_info;
	struct thread *curThread = thread_current();
	if(pml4_is_dirty(curThread->pml4, page->va)){
		file_write_at(f_info->file, page->frame->kva, f_info->read_bytes, f_info->offset);
	}
	file_page->is_in_mem = false;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page = &page->file;
	struct file_info *f_info = &file_page->f_info;
	struct thread *curThread = thread_current();
	if(file_page->is_in_mem)
		file_backed_swap_out(page);
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
}

/* Do the munmap */
void
do_munmap (void *addr) {
}
