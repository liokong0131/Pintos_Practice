/* vm.c: Generic interface for virtual memory objects. */

#include <string.h>
#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "vm/file.h"
#include "threads/mmu.h"
#define VM

struct hash *frame_table;

/* my implement functions */
uint64_t page_hash_create(const struct hash_elem *e, void *aux);
bool page_cmp_hash(const struct hash_elem *a, const struct hash_elem *b, void *aux);
static void spte_destroy(struct hash_elem *e, void *aux UNUSED);

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
	hash_init(frame_table, &frame_hash_create, &frame_cmp_hash, NULL);
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

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *page = (struct page *)malloc(sizeof(struct page));
		if(page == NULL){
			goto err;
		}

		switch(type){
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
		page->frame_table = frame_table;
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
	target.va = va;

	struct hash_elem *e = hash_find(&spt->hash_table, &target.h_elem);
	if(e != NULL)
		page = hash_entry(e, struct page, h_elem);

	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	int succ = false;
	if(hash_insert(&spt->hash_table, &page->h_elem) == NULL)
		succ = true;

	return succ;
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

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	struct frame *frame = NULL;
	/* TODO: Fill this function. */

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	return vm_do_claim_page (page);
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
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	/* TODO: Fill this function */

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

	return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	hash_init(&spt->hash_table, &page_hash_create, &page_cmp_hash, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src) {
	struct hash_iterator iter;
	hash_first(&iter, &src->hash_table);
	while(hash_next(&iter)){
		struct page *parent_page = hash_entry(hash_cur(&iter), struct page, h_elem);

		// struct page copy
		struct page *child_page = (struct page *)malloc(sizeof(struct page));;
		if (child_page == NULL) {
			PANIC("(in spt copy child_page malloc) Fail");
		}
		memcpy(child_page, parent_page, sizeof(struct page));
		
		// mem (or disk) page copy
		switch(page_get_type(parent_page)){
		case VM_ANON:
			struct anon_page *anon_parent_page = &parent_page->anon;
			struct anon_page *anon_child_page = &child_page->anon;
			if(anon_parent_page->is_in_mem){
				void *upage = palloc_get_page(PAL_USER | PAL_ZERO);
				if (upage == NULL) {
					PANIC("(in spt copy anon upage) Fail");
				}

				memcpy(upage, parent_page->frame->kva, PGSIZE);

				struct frame *child_frame = malloc(sizeof(struct frame));
				if (child_frame == NULL) {
					PANIC("(in spt copy anon frame) Fail");
				}
				child_frame->kva = upage;
				child_frame->page = child_page;
				child_frame->ref_cnt = parent_page->frame->ref_cnt;
				hash_insert(frame_table, &child_frame->h_elem);

				child_page->frame = child_frame;
				anon_child_page->is_in_mem = true;
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
				anon_child_page->is_in_mem = false;
			}
			if(!spt_insert_page(dst, child_page)){
				PANIC("(in spt copy insert page) Fail");
			}
			break;
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
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_destroy(&spt->hash_table, spte_destroy);
}

/* my implement functions */
uint64_t page_hash_create(const struct hash_elem *e, void *aux UNUSED){
	struct page *page = hash_entry(e, struct page, h_elem);
	return hash_bytes(&page->va, sizeof(page->va));
}

bool page_cmp_hash(const struct hash_elem *a,
		const struct hash_elem *b,
		void *aux){
	struct page *page_a = hash_entry(a, struct page, h_elem);
	struct page *page_b = hash_entry(b, struct page, h_elem);
	return page_a->va < page_b->va;
}

uint64_t frame_hash_create(const struct hash_elem *e, void *aux UNUSED){
	struct frame *frame = hash_entry(e, struct frame, h_elem);
	return hash_bytes(&frame->kva, sizeof(frame->kva));
}

bool frame_cmp_hash(const struct hash_elem *a,
		const struct hash_elem *b,
		void *aux){
	struct frame *frame_a = hash_entry(a, struct frame, h_elem);
	struct frame *frame_b = hash_entry(b, struct frame, h_elem);
	return frame_a->kva < frame_b->kva;
}

// hash_action_func
static void
spte_destroy(struct hash_elem *e, void *aux UNUSED){
	struct page *page = hash_entry(e, struct page, h_elem);
	vm_dealloc_page(page);
}


