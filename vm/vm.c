/* vm.c: Generic interface for virtual memory objects. */

#include <string.h>
#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "vm/file.h"
#include "threads/mmu.h"
#include "threads/synch.h"
//#define VM

#define MAX_STACK_SIZE (1 << 20)

static struct list frame_table;

/* my implement functions */
unsigned page_hash_create(const struct hash_elem *e, void *aux UNUSED);
bool page_cmp_hash(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);
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
	list_init(&frame_table);
	//lock_init(&frame_lock);
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
		page->f_info = NULL;

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
		page->aux = NULL;

		if(type & VM_FILE_INFO){
			page->f_info = (struct file_info *)aux;
		}
		
		// if(type & VM_STACK){
		// 	struct intr_frame *f = &thread_current()->tf;
		// 	f->rsp -= PGSIZE;
		// }

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
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	struct thread *curThread = thread_current();
	/* TODO: The policy for eviction is up to you. */
	lock_acquire(curThread->swap_lock);
	if(!list_empty(&frame_table)){
		while(true){
			victim = list_entry(list_begin(&frame_table), struct frame, l_elem);
			if(!pml4_is_accessed(curThread->pml4, victim->page->va)){
				break;
			} else{
				pml4_set_accessed(curThread->pml4, victim->page->va, false);
				list_remove(&victim->l_elem);
				list_push_back(&frame_table, &victim->l_elem);
			}
		}
	}
	lock_release(thread_current()->swap_lock);
	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */
	swap_out(victim->page);
	lock_acquire(thread_current()->swap_lock);
	list_remove(&victim->l_elem);
	lock_release(thread_current()->swap_lock);

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
	frame = (struct frame *)malloc(sizeof(struct frame));
	frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);
	frame->page = NULL;

	if(frame->kva == NULL){
		struct frame *victim = vm_evict_frame();
		frame->kva = victim->kva;
		pml4_clear_page(thread_current()->pml4, victim->page->va);
		memset(frame->kva, 0, PGSIZE);
		lock_acquire(thread_current()->swap_lock);
		list_remove(&victim->l_elem);
		lock_release(thread_current()->swap_lock);
		free(victim);
	}

	lock_acquire(thread_current()->swap_lock);
	list_push_back(&frame_table, &frame->l_elem);
	lock_release(thread_current()->swap_lock);

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);

	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr, void *rsp) {
	vm_alloc_page(VM_ANON | VM_STACK, addr, true);
}
// static void
// vm_stack_growth (void *addr, void *rsp) {
// 	struct thread *curThread = thread_current();
// 	void *a = addr;
// 	while(a <= pg_round_down(rsp)){
// 		if(spt_find_page(&curThread->spt, a) == NULL){
// 			vm_alloc_page(VM_ANON | VM_STACK, a, true);
// 			vm_do_claim_page (spt_find_page(&curThread->spt, a));
// 		}
// 		printf("%p\n", a);
// 		a += PGSIZE;
// 	}
// }

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

#ifdef DEBUG
	//printf("addr : %p, %d, %d, %d\n", addr, user, write, not_present);
#endif

	if(is_kernel_vaddr(addr) || addr == NULL)
		return false;
	if(not_present){
		void *user_rsp = user ? f->rsp : thread_current()->user_rsp;
		if (is_in_USER_STACK(addr) && user_rsp - 8 <= addr) {
			vm_stack_growth (pg_round_down(addr), user_rsp);
		}
		if((page = spt_find_page(spt, addr)) == NULL){
			return false;
		}
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
	ASSERT(page != NULL);
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

		switch(VM_TYPE (parent_page->operations->type)){
		case VM_UNINIT:
			if (!vm_alloc_page_with_initializer(page_get_type(parent_page) | VM_COPY, parent_page->va, parent_page->writable, parent_page->uninit.init, parent_page->uninit.aux)) {
				return false;
			}
			struct page *child_page = spt_find_page(dst, parent_page->va);
			if(parent_page->f_info != NULL){
				child_page->f_info = (struct file_info *)malloc(sizeof(struct file_info));
				child_page->f_info->file = parent_page->f_info->file;
				child_page->f_info->offset = parent_page->f_info->offset;
				child_page->f_info->read_bytes = parent_page->f_info->read_bytes;
				child_page->f_info->zero_bytes = parent_page->f_info->zero_bytes;
			}
			break;
		case VM_ANON:{
			struct copy_info *c_info = (struct copy_info *)malloc(sizeof(struct copy_info));
			c_info->parent_page = parent_page;
			void *aux = c_info;
			if (!vm_alloc_page_with_initializer(VM_ANON, parent_page->va, parent_page->writable, anon_page_copy, aux)){
				free(c_info);
				return false;
			}
			
			struct page *child_page = spt_find_page(dst, parent_page->va);
			if(!vm_claim_page(parent_page->va)) {
				free(c_info);
				return false;
			}
			free(c_info);
			break;
		}
		case VM_FILE:{
			struct copy_info *c_info = (struct copy_info *)malloc(sizeof(struct copy_info));
			c_info->parent_page = parent_page;
			void *aux = c_info;
			if (!vm_alloc_page_with_initializer(VM_FILE, parent_page->va, parent_page->writable, file_page_copy, aux)){
				free(c_info);
				return false;
			}
			struct page *child_page = spt_find_page(dst, parent_page->va);
			if(!vm_claim_page(parent_page->va)) {
				free(c_info);
				return false;
			}
			free(c_info);
			break;
		}
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
	vm_dealloc_page(page);
}

bool is_in_USER_STACK(void *uaddr){
	return USER_STACK - MAX_STACK_SIZE <= uaddr  &&
			uaddr < USER_STACK;
}