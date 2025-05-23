#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
#include "devices/disk.h"
struct page;
enum vm_type;

struct anon_page {
    disk_sector_t swap_sector;
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);
bool anon_page_copy (struct page *page, void *aux);
#endif
