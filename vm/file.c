/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/mmu.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
static bool lazy_mmap_segment (struct page *page, void *aux);
static bool mmap_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable);
unsigned mmap_hash_create(const struct hash_elem *e, void *aux UNUSED);
bool mmap_cmp_hash(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
struct mmap_info * mmap_find (struct hash *mmap_table, void *va);
bool mmap_insert (struct hash *mmap_table, struct mmap_info *m_info);
void mmap_remove (struct hash *mmap_table, struct mmap_info *m_info);
static void mmape_destroy(struct hash_elem *e, void *aux UNUSED);

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
	struct thread *curThread = thread_current();
	file_disk = disk_get(0, 1);
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page = &page->file;
	struct file_info *f_info = &page->f_info;
	off_t bytes_read = file_read_at(f_info->file, kva, f_info->read_bytes, f_info->offset);
	if(bytes_read != (off_t)f_info->read_bytes){
		return false;
	}

	memset(kva + f_info->read_bytes, 0, f_info->zero_bytes);
	page->is_in_mem = true;
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page = &page->file;
	struct file_info *f_info = &page->f_info;
	struct thread *curThread = thread_current();
	if(pml4_is_dirty(curThread->pml4, page->va)){
		off_t bytes_write = file_write_at(f_info->file, page->frame->kva, f_info->read_bytes, f_info->offset);
		if(bytes_write != (off_t)f_info->read_bytes){
			return false;
		}
	}
	page->is_in_mem = false;
	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page = &page->file;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	if(file_length(file) == 0) return NULL;

	size_t read_bytes = length;
	size_t zero_bytes = 0;

	if(length > file_length(file)){
		read_bytes = file_length(file);
	}
	zero_bytes = PGSIZE - read_bytes % PGSIZE;

	mmap_segment(file, offset, addr, read_bytes, zero_bytes, writable);
	return addr;
}

/* Do the munmap */
void
do_munmap (void *addr) {
	struct thread *curThread = thread_current();
	struct supplemental_page_table *spt = &curThread->spt;
	struct hash *mmap_table = &curThread->mmap_table;
	struct mmap_info *m_info = mmap_find(mmap_table, addr);
	
	for(int i=0; i<m_info->pages; i++){
		struct page *page = spt_find_page(spt, addr + PGSIZE * i);
		if(i == 0){
			file_close(page->f_info->file);
		}
		if(page->frame != NULL){
			file_backed_swap_out(page);
			palloc_free_page(page->frame->kva);
			list_remove(&page->frame->l_elem);
			free(page->frame);
		}
		pml4_clear_page(thread_current()->pml4, page->va);
		spt_remove_page(spt, page);
	}
	
	mmap_remove(mmap_table, m_info);
}
/* my implement functions */
static bool
lazy_mmap_segment (struct page *page, void *aux) {
	struct file_info *f_info = (struct file_info *)aux;
	off_t bytes_read = file_read_at(f_info->file, page->frame->kva, f_info->read_bytes, f_info->offset);
	if(bytes_read != (off_t)f_info->read_bytes){
		return false;
	}

	memset(page->frame->kva + f_info->read_bytes, 0, f_info->zero_bytes);
	page->is_in_mem = true;
	return true;
}

static bool
mmap_segment (struct file *file, off_t ofs, uint8_t *upage,
		uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (ofs % PGSIZE == 0);

	struct hash *mmap_table = &thread_current()->mmap_table;
	struct mmap_info *m_info = (struct mmap_info *)malloc(sizeof(struct mmap_info));
	m_info->start_uaddr = upage;
	m_info->pages = 0;
	struct file *nfile = file_reopen(file);

	while (read_bytes > 0 || zero_bytes > 0) {
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		struct file_info *f_info = (struct file_info *)malloc(sizeof(struct file_info));
		f_info->file = nfile;
		f_info->offset = ofs;
		f_info->read_bytes = page_read_bytes;
		f_info->zero_bytes = page_zero_bytes;

		void *aux = f_info;
		if (!vm_alloc_page_with_initializer (VM_FILE | VM_FILE_INFO, upage,
					writable, lazy_mmap_segment, aux)){
			free(f_info);
			free(m_info);
			return false;
		}
		m_info->pages++;
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
		ofs += page_read_bytes;
	}
	if(!mmap_insert(mmap_table, m_info)){
		free(m_info);
		return false;
	}
	return true;
}

void
mmap_table_init (struct hash *mmap_table) {
	hash_init(mmap_table, mmap_hash_create, mmap_cmp_hash, NULL);
}


unsigned mmap_hash_create(const struct hash_elem *e, void *aux UNUSED){
	struct mmap_info *m_info = hash_entry(e, struct mmap_info, h_elem);
	return hash_bytes(&m_info->start_uaddr, sizeof(void *));
}

bool mmap_cmp_hash(const struct hash_elem *a,
		const struct hash_elem *b,
		void *aux UNUSED){
	struct mmap_info *m_info_a = hash_entry(a, struct mmap_info, h_elem);
	struct mmap_info *m_info_b = hash_entry(b, struct mmap_info, h_elem);
    return m_info_a->start_uaddr < m_info_b->start_uaddr;
}

struct mmap_info *
mmap_find (struct hash *mmap_table, void *va) {
	struct mmap_info *m_info = NULL;
	struct mmap_info target;
	target.start_uaddr = pg_round_down(va);

	struct hash_elem *e = hash_find(mmap_table, &target.h_elem);
	if(e != NULL)
		m_info = hash_entry(e, struct mmap_info, h_elem);

	return m_info;
}

bool
mmap_insert (struct hash *mmap_table, struct mmap_info *m_info) {
	return hash_insert(mmap_table, &m_info->h_elem) == NULL;
}

void
mmap_remove (struct hash *mmap_table, struct mmap_info *m_info) {
	hash_delete(mmap_table, &m_info->h_elem);
	free(m_info);
}

bool
mmap_table_copy (struct hash *dst,
		struct hash *src) {
	struct hash_iterator iter;
	hash_first(&iter, src);
	while(hash_next(&iter)){
		struct mmap_info *parent_m_info = hash_entry(hash_cur(&iter), struct page, h_elem); 
		struct mmap_info *child_m_info = (struct mmap_info *)malloc(sizeof(struct mmap_info));
		if(child_m_info == NULL){
			return false;
		}
		child_m_info->pages = parent_m_info->pages;
		child_m_info->start_uaddr = parent_m_info->start_uaddr;
		mmap_insert(dst, child_m_info);
	}
	return true;
}

void
mmap_table_kill (struct hash *mmap_table) {
	hash_clear (mmap_table, mmape_destroy);
}

static void
mmape_destroy(struct hash_elem *e, void *aux UNUSED){
	struct mmap_info *m_info = hash_entry(e, struct mmap_info, h_elem);
	free(m_info);
}
