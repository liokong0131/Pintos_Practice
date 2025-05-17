/* vm.c: Generic interface for virtual memory objects. */

#include <string.h>
#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "vm/file.h"
#include "threads/mmu.h"
#include "threads/synch.h"
//#define VM

static struct list frame_table;
static struct lock spt_lock;
/* my implement functions */
unsigned page_hash_create(const struct hash_elem *e, void *aux UNUSED);
bool page_cmp_hash(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
static void spte_destroy(struct hash_elem *e, void *aux UNUSED);
static bool spte_copy(struct hash_elem *e, void *aux UNUSED);
static bool page_copy (struct page *page, void *aux)
/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	list_init(&frame_table);
	lock_init(&spt_lock);
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current ()->spt;
	ASSERT(spt != NULL);
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *page = (struct page *)malloc(sizeof(struct page));
		if(page == NULL){
			goto err;
		}

		if(type & VM_COPY){
			init = spte_copy;
		}
		//else if(type & VM_STACK){}

		switch(VM_TYPE(type)){
		case VM_ANON:
			uninit_new(page, upage, init, type, aux, anon_initializer);
			break;
		case VM_FILE:
			uninit_new(page, upage, init, type, aux, file_backed_initializer);
			break;
		default:
			goto err;
		}

		page->writable = writable;

		/* TODO: Insert the page into the spt. */
		if (!spt_insert_page(spt, page)) {
			free(page);
			goto err;
		}

		return true;
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page *page = NULL;
	/* TODO: Fill this function. */
	struct page target;
	target.va = pg_round_down(va);

	struct hash_elem *e = hash_find(&spt->hash_table, &target.h_elem);
	if(e != NULL)
		page = hash_entry(e, struct page, h_elem);

	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	ASSERT(spt != NULL);
	return hash_insert(&spt->hash_table, &page->h_elem) == NULL;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	hash_delete(&spt->hash_table, &page->h_elem);
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */
	if(!list_empty(&frame_table)){
		while(true){
			victim = list_entry(list_begin(&frame_table), struct frame, l_elem);
			if(victim->ref_cnt == 0){
				break;
			} else{
				victim->ref_cnt = 0;
				list_push_back(&frame_table, list_pop_front(&frame_table));
			}
		}
	}
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	swap_out(victim->page);
	list_remove(&victim->l_elem);
	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */
	frame = malloc(sizeof(struct frame));
	frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);
	frame->page = NULL;
	frame->ref_cnt = 0;

	if(frame->kva == NULL){
		struct frame *victim = vm_evict_frame();
		frame->kva = victim->kva;
		memset(frame->kva, 0, PGSIZE);
		free(victim);
	}

	list_push_back(&frame_table, &frame->l_elem);

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f, void *addr,
		bool user, bool write, bool not_present) {
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	if(!user || addr == NULL)
		return false;

	if(not_present){
		if((page = spt_find_page(spt, addr)) == NULL)
			return false;
		
		if (write && !page->writable) {
			return false;
		}

		return vm_do_claim_page (page);
	}
	
	return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va) {
	struct page *page = NULL;
	/* TODO: Fill this function */
	struct thread *curThread = thread_current();
	if((page = spt_find_page(&curThread->spt, va)) == NULL)
		return false;
	
	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	struct thread *curThread = thread_current();
	void *upage = page->va;
	void *kva = frame->kva;

	if(!pml4_set_page(curThread->pml4, upage, kva, page->writable)){
		palloc_free_page(kva);
		free(frame);
		return false;
	}

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	hash_init(&spt->hash_table, page_hash_create, page_cmp_hash, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src) {
	struct hash_iterator iter;
	hash_first(&iter, &src->hash_table);
	while(hash_next(&iter)){
		struct page *parent_page = hash_entry(hash_cur(&iter), struct page, h_elem);
		struct copy_info *c_info = (struct copy_info *)malloc(sizeof(struct copy_info));
		c_info->parent_page = parent_page;
		void *aux = c_info; 
		// struct page copy
		if (!(vm_alloc_page_with_initializer(VM_ANON | VM_COPY, parent_page->va, parent_page->writable, page_copy, aux)
			 && vm_claim_page(parent_page->va))) {
			return false;
		}
		
		// mem (or disk) page copy
		switch(page_get_type(parent_page)){
		case VM_ANON: {
			struct anon_page *anon_parent_page = &parent_page->anon;
			struct anon_page *anon_child_page = &child_page->anon;
			if(anon_parent_page->is_in_mem){
				if(!vm_do_claim_page(child_page)){
					PANIC("(in spt copy claim page) Fail");
				}
				struct frame *child_frame = child_page->frame;
				memcpy(child_frame->kva, parent_page->frame->kva, PGSIZE);
				child_frame->page = child_page;
				child_frame->ref_cnt = parent_page->frame->ref_cnt;

				//anon_child_page->is_in_mem = true;
			}else{
				size_t cache_idx = 0;
				struct disk *swap_disk = disk_get(1, 1);
				void *buffer = malloc(DISK_SECTOR_SIZE);
				for(int i=0; i<8; i++){
					anon_child_page->swap_sectors[i] = bitmap_scan_and_flip(anon_child_page->swap_table, cache_idx, 1, true);
					if(anon_child_page->swap_sectors[i] == BITMAP_ERROR){
						PANIC("(in spt copy anon) Fail: swap_sectors index %d", i);
					}

					cache_idx = anon_child_page->swap_sectors[i];
					
					if(buffer == NULL){
						PANIC("(in spt copy anon malloc) Fail");
					}
					disk_read(swap_disk, anon_parent_page->swap_sectors[i], buffer);
					disk_write(swap_disk, cache_idx, buffer);
				}
				free(buffer);
				//anon_child_page->is_in_mem = false;
			}
			break;
		}
		case VM_FILE:
		case VM_UNINIT:
		default:
			break;
		}
	}
	return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_clear (&spt->hash_table, spte_destroy);
}

void
supplemental_page_table_destroy (struct supplemental_page_table *spt){
	hash_destroy (&spt->hash_table, spte_destroy);
}

/* my implement functions */
unsigned page_hash_create(const struct hash_elem *e, void *aux UNUSED){
	struct page *page = hash_entry(e, struct page, h_elem);
	return hash_bytes(&page->va, sizeof(void *));
}

bool page_cmp_hash(const struct hash_elem *a,
		const struct hash_elem *b,
		void *aux UNUSED){
	struct page *page_a = hash_entry(a, struct page, h_elem);
	struct page *page_b = hash_entry(b, struct page, h_elem);
    return page_a->va < page_b->va;
}

// hash_action_func
static void
spte_destroy(struct hash_elem *e, void *aux UNUSED){
	struct page *page = hash_entry(e, struct page, h_elem);
	if(page->frame != NULL){
		list_remove(&page->frame->l_elem);
		free(page->frame);
	}
	vm_dealloc_page(page);
}

static bool
spte_copy (struct hash_elem *e, void *aux UNUSED) {
	struct page *page = hash_entry(e, struct page, h_elem);
	hash_apply(&dst->hash_table, spte_copy);
	while(hash_next(&iter)){
		struct page *parent_page = hash_entry(hash_cur(&iter), struct page, h_elem);

		// struct page copy
		if (!vm_alloc_page(VM_COPY, parent_page->va, parent_page->writable)) {
			PANIC("(in spt copy child_page malloc) Fail");
		}
		struct page *child_page = spt_find_page(src, parent_page->va);
		memcpy(child_page, parent_page, sizeof(struct page));
		
		// mem (or disk) page copy
		switch(page_get_type(parent_page)){
		case VM_ANON: {
			struct anon_page *anon_parent_page = &parent_page->anon;
			struct anon_page *anon_child_page = &child_page->anon;
			if(anon_parent_page->is_in_mem){
				if(!vm_do_claim_page(child_page)){
					PANIC("(in spt copy claim page) Fail");
				}
				struct frame *child_frame = child_page->frame;
				memcpy(child_frame->kva, parent_page->frame->kva, PGSIZE);
				child_frame->page = child_page;
				child_frame->ref_cnt = parent_page->frame->ref_cnt;

				//anon_child_page->is_in_mem = true;
			}else{
				size_t cache_idx = 0;
				struct disk *swap_disk = disk_get(1, 1);
				void *buffer = malloc(DISK_SECTOR_SIZE);
				for(int i=0; i<8; i++){
					anon_child_page->swap_sectors[i] = bitmap_scan_and_flip(anon_child_page->swap_table, cache_idx, 1, true);
					if(anon_child_page->swap_sectors[i] == BITMAP_ERROR){
						PANIC("(in spt copy anon) Fail: swap_sectors index %d", i);
					}

					cache_idx = anon_child_page->swap_sectors[i];
					
					if(buffer == NULL){
						PANIC("(in spt copy anon malloc) Fail");
					}
					disk_read(swap_disk, anon_parent_page->swap_sectors[i], buffer);
					disk_write(swap_disk, cache_idx, buffer);
				}
				free(buffer);
				//anon_child_page->is_in_mem = false;
			}
			break;
		}
		case VM_FILE:
		case VM_UNINIT:
		default:
			break;
		}
	}
	return true;
}

static bool
page_copy (struct page *page, void *aux){
	struct file_info *f_info = (struct file_info *)aux;

	off_t bytes_read = file_read_at(f_info->file, page->frame->kva, f_info->read_bytes, f_info->offset);
	if(bytes_read != (off_t)f_info->read_bytes){
		return false;
	}

	memset(page->frame->kva + f_info->read_bytes, 0, f_info->zero_bytes);
	
	return true;
}