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

enum syscall_status {
	READ,
	WRITE,     
	OTHERS
};

typedef int pid_t;

void check_valid_uaddr(const uint64_t *addr);
bool is_valid_fd(int fd, enum syscall_status stat);

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
syscall_handler (struct intr_frame *if_ UNUSED) {
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
		if_->R.rax = mmap(if_->R.rdi, if_->R.rsi);
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
	if((fn_copy = palloc_get_page(0)) == NULL)
		exit(-1);

	strlcpy(fn_copy, cmd_line, PGSIZE);

	return process_exec(fn_copy);
}
int wait (pid_t pid){
	return process_wait(pid);
}

bool create (const char *file, unsigned initial_size){
	check_valid_uaddr(file);
	return filesys_create(file, initial_size);
}
bool remove (const char *file){
	check_valid_uaddr(file);
	return filesys_remove(file);
}
int open (const char *file){
	check_valid_uaddr(file);
	int fd;
	struct file *open_file;
	if((open_file = filesys_open(file)) == NULL)
		return -1;

	if ((fd = insert_file_to_fdt(open_file)) == -1)
		file_close(open_file);

	//file_deny_write(open_file);
#ifdef DEBUG
	//printf("(open) fd: %d\n", fd);
#endif

	return fd;
}
int filesize (int fd){
	struct thread *curThread = thread_current ();
	if(!is_valid_fd (fd, OTHERS)) return -1;

	struct file *file = curThread->fdt[fd];
	return file_length (file);
}
int read (int fd, void *buffer, unsigned length){

#ifdef DEBUG
	//printf("\n\n(read) fd: %d\n\n", fd);
#endif
	check_valid_uaddr(buffer);
	if(!is_valid_fd (fd, READ)) return -1;
	struct thread *curThread = thread_current ();
	if(fd == 0){
		return input_getc();
	} else{
		struct file *file = curThread->fdt[fd];
		file_deny_write(file);
		return file_read(file, buffer, length);
	}
}
int write (int fd, const void *buffer, unsigned length){

#ifdef DEBUG
	printf("\n(write) fd, buffer : %d, %s\n", fd, buffer);
#endif

	check_valid_uaddr(buffer);

#ifdef DEBUG
	//printf("\n(write) after check uaddr\n");
#endif

	if(!is_valid_fd (fd, WRITE)) return -1;

	struct thread *curThread = thread_current ();



	if(fd == 1){
		putbuf(buffer, length);
		return length;
	}else{
		struct file *file = curThread->fdt[fd];
		//file_allow_write(file);
#ifdef DEBUG
		//printf("\n(write) writable : %d\n", file->deny_write);
#endif

		return file_write(file, buffer, length);
	}
}
void seek (int fd, unsigned position){
	struct thread *curThread = thread_current ();
	if(!is_valid_fd (fd, OTHERS)) return;
	struct file *file = curThread->fdt[fd];
	file_seek(file, position);
	if (position == 0) file_allow_write(file);
}	
unsigned tell (int fd){
	struct thread *curThread = thread_current ();
	if(!is_valid_fd (fd, OTHERS)) return;
	struct file *file = curThread->fdt[fd];
	return file_tell(file);
}
void close (int fd){
	struct thread *curThread = thread_current ();
	if(!is_valid_fd (fd, OTHERS)) return;
	struct file *file = curThread->fdt[fd];
	file_close(file);

	curThread->fdt[fd] = NULL;
	curThread->fdt_cur = fd;
	curThread->num_files--;
}

int dup2(int oldfd, int newfd){
	return;
}

void *mmap(int fd, void *addr){

}

void munmap(){

}


int insert_file_to_fdt(struct file *file){
	struct thread *curThread = thread_current ();

#ifdef DEBUG
	//printf("(iff) fdt_size & name: %d, %s\n", curThread->fdt_cur, curThread->name);
#endif

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
	if (addr == NULL || !is_user_vaddr(addr) || pml4_get_page(thread_current()->pml4, addr) == NULL)
    	exit(-1);
}

bool is_valid_fd(int fd, enum syscall_status stat){
	struct thread *curThread = thread_current ();
	if(2 <= fd && fd <= FDT_LIMIT && curThread->fdt[fd] != NULL)
		return true;
	else if (stat == WRITE && fd == 1) return true;
	else if (stat == READ && fd == 0 ) return true;
	else return false;
}