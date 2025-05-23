//#define DEBUG
#define USERPROG
#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "threads/palloc.h"
#include "intrinsic.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "vm/file.h"

enum syscall_status {
	READ,
	WRITE,     
	OTHERS
};

typedef int pid_t;

void check_valid_uaddr(const uint64_t *addr);
bool is_valid_fd(int fd, enum syscall_status stat);
bool is_valid_uaddr(const uint64_t *addr, size_t length);
void check_writable_addr(const uint64_t *addr, unsigned length);
bool is_overlap(const uint64_t *addr, size_t length);
bool is_valid_offset(off_t offset);
bool is_writable_addr(const uint64_t *addr, unsigned length);

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

void halt (void);
void exit (int status);
pid_t fork (const char *thread_name, struct intr_frame *if_);
int exec (const char *cmd_line);
int wait (pid_t pid);

bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
int write (int fd, const void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
int dup2(int oldfd, int newfd);
void *mmap (void *addr, size_t length, int writable, int fd, off_t offset);
void munmap (void *addr);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *if_) {
	// TODO: Your implementation goes here.
	// printf ("system call!\n");
	// thread_exit ();
	/*
	register uint64_t *num asm ("rax") = (uint64_t *) num_;
	register uint64_t *a1 asm ("rdi") = (uint64_t *) a1_;
	register uint64_t *a2 asm ("rsi") = (uint64_t *) a2_;
	register uint64_t *a3 asm ("rdx") = (uint64_t *) a3_;
	register uint64_t *a4 asm ("r10") = (uint64_t *) a4_;
	register uint64_t *a5 asm ("r8") = (uint64_t *) a5_;
	register uint64_t *a6 asm ("r9") = (uint64_t *) a6_;
	*/
	char *fn_copy;
	int size;
	thread_current()->user_rsp = if_->rsp;
	switch (if_->R.rax) {
	case SYS_HALT:
		halt(); break;
	case SYS_EXIT:
		exit(if_->R.rdi); break;
	case SYS_FORK:
		if_->R.rax = fork(if_->R.rdi, if_);
		break;
	case SYS_EXEC:
		if (exec(if_->R.rdi) == -1)
#ifdef DEBUG
			printf("exec value is -1\n");
#endif
			exit(-1);
		break;
	case SYS_WAIT:
		if_->R.rax = process_wait(if_->R.rdi);
		break;
	case SYS_CREATE:
		if_->R.rax = create(if_->R.rdi, if_->R.rsi);
		break;
	case SYS_REMOVE:
		if_->R.rax = remove(if_->R.rdi);
		break;
	case SYS_OPEN:
		if_->R.rax = open(if_->R.rdi);
		break;
	case SYS_FILESIZE:
		if_->R.rax = filesize(if_->R.rdi);
		break;
	case SYS_READ:
		if_->R.rax = read(if_->R.rdi, if_->R.rsi, if_->R.rdx);
		break;
	case SYS_WRITE:
		if_->R.rax = write(if_->R.rdi, if_->R.rsi, if_->R.rdx);
		break;
	case SYS_SEEK:
		seek(if_->R.rdi, if_->R.rsi); break;
	case SYS_TELL:
		if_->R.rax = tell(if_->R.rdi);
		break;
	case SYS_CLOSE:
		close(if_->R.rdi); break;
	case SYS_DUP2:
		if_->R.rax = dup2(if_->R.rdi, if_->R.rsi);
		break;
	case SYS_MMAP:
		if_->R.rax = mmap(if_->R.rdi, if_->R.rsi, if_->R.rdx, if_->R.r10, if_->R.r8);
		break;
	case SYS_MUNMAP:
		munmap(if_->R.rdi);
		break;
	default:
		exit(-1); break;
	}
}

void halt (void){
	power_off();
}
void exit (int status){
	thread_current()->exit_status = status;
	thread_exit();
	//process_exit();
	// if(status == 0) printf("success");
	// else printf("error");
}
pid_t fork (const char *thread_name, struct intr_frame *if_){
	//check_valid_uaddr(thread_name);
	//check_valid_uaddr(if_);
	return process_fork(thread_name, if_);
}
int exec (const char *cmd_line){
	check_valid_uaddr(cmd_line);

	char *fn_copy;
	if((fn_copy = palloc_get_page(0)) == NULL){
#ifdef DEBUG
		printf("exec fn_copy NULL\n");
#endif
		exit(-1);
	}
	strlcpy(fn_copy, cmd_line, PGSIZE);

	return process_exec(fn_copy);
}
int wait (pid_t pid){
	return process_wait(pid);
}

bool create (const char *file, unsigned initial_size){
	check_valid_uaddr(file);
	lock_acquire(thread_current()->filesys_lock);
	bool success = filesys_create(file, initial_size);
	lock_release(thread_current()->filesys_lock);
	return success;
}
bool remove (const char *file){
	check_valid_uaddr(file);
	lock_acquire(thread_current()->filesys_lock);
	bool success = filesys_remove(file);
	lock_release(thread_current()->filesys_lock);
	return success;
}
int open (const char *file){
	check_valid_uaddr(file);
	int fd;
	struct file *open_file;
	lock_acquire(thread_current()->filesys_lock);
	if((open_file = filesys_open(file)) == NULL){
		lock_release(thread_current()->filesys_lock);
		return -1;
	}
		
	if ((fd = insert_file_to_fdt(open_file)) == -1)
		file_close(open_file);
	lock_release(thread_current()->filesys_lock);
	return fd;
}
int filesize (int fd){
	struct thread *curThread = thread_current ();
	if(!is_valid_fd (fd, OTHERS)) return -1;

	struct file *file = curThread->fdt[fd];
	lock_acquire(thread_current()->filesys_lock);
	off_t result = file_length (file);
	lock_release(thread_current()->filesys_lock);
	return result;
}
int read (int fd, void *buffer, unsigned length){
	check_valid_uaddr(buffer);
	check_writable_addr(buffer, length);
	if(!is_valid_fd (fd, READ)) return -1;
	
	struct thread *curThread = thread_current ();
	
	if(fd == 0){
		int result = input_getc();
		return result;
	} else{
		void *kva;
		struct file *file = curThread->fdt[fd];
		lock_acquire(thread_current()->filesys_lock);
		file_deny_write(file);
		off_t result = file_read(file, buffer, length);
		lock_release(thread_current()->filesys_lock);
		return result;
	}
}
int write (int fd, const void *buffer, unsigned length){
	check_valid_uaddr(buffer);
	if(!is_valid_fd (fd, WRITE)) return -1;

	struct thread *curThread = thread_current ();
	if(fd == 1){
		putbuf(buffer, length);
		return 0;
	}else{
		struct file *file = curThread->fdt[fd];
		lock_acquire(thread_current()->filesys_lock);
		off_t result = file_write(file, buffer, length);
		lock_release(thread_current()->filesys_lock);
		return result;
	}
}
void seek (int fd, unsigned position){
	struct thread *curThread = thread_current ();
	if(!is_valid_fd (fd, OTHERS)) return;
	struct file *file = curThread->fdt[fd];
	lock_acquire(thread_current()->filesys_lock);
	file_seek(file, position);
	if (position == 0) file_allow_write(file);
	lock_release(thread_current()->filesys_lock);
}	
unsigned tell (int fd){
	struct thread *curThread = thread_current ();
	if(!is_valid_fd (fd, OTHERS)) return;
	struct file *file = curThread->fdt[fd];
	lock_acquire(thread_current()->filesys_lock);
	off_t result = file_tell(file);
	lock_release(thread_current()->filesys_lock);
	return result;
}
void close (int fd){
	struct thread *curThread = thread_current ();
	if(!is_valid_fd (fd, OTHERS)) return;
	struct file *file = curThread->fdt[fd];
	lock_acquire(thread_current()->filesys_lock);
	file_close(file);
	lock_release(thread_current()->filesys_lock);
	curThread->fdt[fd] = NULL;
	curThread->fdt_cur = fd;
	curThread->num_files--;
}

int dup2(int oldfd, int newfd){
	return;
}

void *mmap (void *addr, size_t length, int writable, int fd, off_t offset){
	if(!is_valid_fd(fd, OTHERS)) return NULL;
	if(!is_valid_uaddr(addr, length)) return NULL;
	if(!is_valid_offset(offset)) return NULL;
	if(length <= 0) return NULL;
	if(is_overlap(addr, length)) return NULL;
	if(!is_writable_addr(addr, length)) return NULL;
	if(spt_find_page(&thread_current()->spt, addr)) return NULL;
	return do_mmap(addr, length, writable, thread_current()->fdt[fd], offset);
}

void munmap(void *addr){
	if(!is_valid_uaddr(addr, 0)) return;
	do_munmap(addr);
}


int insert_file_to_fdt(struct file *file){
	struct thread *curThread = thread_current ();

	while(curThread->fdt_cur < FDT_LIMIT && curThread->fdt[curThread->fdt_cur] != NULL){
		curThread->fdt_cur++;
	}

	if(curThread->fdt_cur >= FDT_LIMIT){
		return -1;
	}

	curThread->fdt[curThread->fdt_cur] = file;
	curThread->num_files++;
  	return curThread->fdt_cur;
}
void check_valid_uaddr(const uint64_t *addr){
	struct thread *curThread = thread_current();
	if (addr == NULL || !is_user_vaddr(addr)){
#ifdef DEBUG
		printf("addr is NULL or addr is kernel\n");
#endif
    	exit(-1);
	}
	if(pml4_get_page(curThread->pml4, addr) == NULL){
		struct page *page = spt_find_page(&curThread->spt, pg_round_down(addr));
		if(page == NULL){
			if(is_in_USER_STACK(addr) && curThread->user_rsp - 8 <= addr) return;
#ifdef DEBUG
			printf("no mapping and no page\n");
#endif
			exit(-1);
		}
	}
}

bool is_valid_uaddr(const uint64_t *addr, size_t length){
	if (addr == NULL || !is_user_vaddr(addr) || addr != pg_round_down(addr) || !is_user_vaddr(addr + length))
    	return false;
	return true;
}

bool is_valid_offset(off_t offset){
	if(offset != pg_round_down(offset))
		return false;
	return true;
}

bool is_valid_fd(int fd, enum syscall_status stat){
	struct thread *curThread = thread_current ();
	if(2 <= fd && fd <= FDT_LIMIT && curThread->fdt[fd] != NULL)
		return true;
	else if (stat == WRITE && fd == 1) return true;
	else if (stat == READ && fd == 0 ) return true;
	else return false;
}

void check_writable_addr(const uint64_t *addr, unsigned length){
	struct page *page = spt_find_page(&thread_current()->spt, pg_round_down(addr));
	if(page == NULL){
		return;
	}
	if(!page->writable){
#ifdef DEBUG
		printf("no writable\n");
#endif
		exit(-1);
	}
	page = spt_find_page(&thread_current()->spt, pg_round_down(addr + length));
	if(page == NULL){
		return;
	}
	if(!page->writable){
#ifdef DEBUG
		printf("no writable\n");
#endif
		exit(-1);
	}
}

bool is_writable_addr(const uint64_t *addr, unsigned length){
	struct page *page = spt_find_page(&thread_current()->spt, pg_round_down(addr));
	if(page == NULL){
		return true;
	}
	if(!page->writable){
#ifdef DEBUG
		printf("no writable\n");
#endif
		return false;
	}
	page = spt_find_page(&thread_current()->spt, pg_round_down(addr + length));
	if(page == NULL){
		return true;
	}
	if(!page->writable){
#ifdef DEBUG
		printf("no writable\n");
#endif
		return false;
	}
}

bool is_overlap(const uint64_t *addr, size_t length){
	for(uint64_t cur_addr = addr; cur_addr < addr + length; cur_addr+=PGSIZE){
		if(mmap_find(&thread_current()->mmap_table, cur_addr) != NULL){
#ifdef DEBUG
			printf("overlapping\n");
#endif
			return true;
		}
	}
	return false;
}