#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"
#include "hash.h"

struct page;
enum vm_type;

struct file_info{
	struct file *file;
	off_t offset;
	size_t read_bytes;
	size_t zero_bytes;
};

struct mmap_info{
	void *start_uaddr;
	int pages;
	struct hash_elem h_elem;
};

struct file_page {
	struct file_info f_info;
};

void vm_file_init (void);
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset);
void do_munmap (void *va);
void mmap_table_init (struct hash *mmap_table);
bool mmap_table_copy (struct hash *dst, struct hash *src);

void mmap_table_kill (struct hash *mmap_table);
#endif
