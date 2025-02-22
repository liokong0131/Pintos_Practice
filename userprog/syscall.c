#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

//################ implement ###################

// void syscall_halt (void);
// void syscall_exit (int status);
// int syscall_exec (const char *cmd_line);
// bool syscall_create (const char *file, unsigned initial_size);
// bool syscall_remove (const char *file);
// tid_t syscall_fork (struct intr_frame *f);
// void syscall_seek (int fd, unsigned position);
// unsigned syscall_tell (int fd);
// int syscall_filesize (int fd);
// int syscall_wait (tid_t tid);
// int syscall_open (const char *file);
// int syscall_read (int fd, void *buffer, unsigned size);
// int syscall_write (int fd, const void *buffer, unsigned size);

//##############################################


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
syscall_handler (struct intr_frame *f UNUSED) {
	// uint64_t syscall_num = f->R.rax;
	// switch (syscall_num) {
	// 	case SYS_HALT:
	// 		syscall_halt ();
	// 		break;
	// 	case SYS_EXIT:
	// 		syscall_exit (f->R.rdi);
	// 		break;
	// 	case SYS_FORK:
	// 		f->R.rax = syscall_fork (f);
	// 		break;
	// 	case SYS_EXEC:
	// 		f->R.rax = syscall_exec (f->R.rdi);
	// 		break;
	// 	case SYS_WAIT:
	// 		f->R.rax = syscall_wait (f->R.rdi);
	// 		break;
	// 	case SYS_CREATE:
	// 		f->R.rax = syscall_create (f->R.rdi, f->R.rsi);
	// 		break;
	// 	case SYS_REMOVE:
	// 		f->R.rax = syscall_remove (f->R.rdi);
	// 		break;
	// 	case SYS_OPEN:
	// 		f->R.rax = syscall_open (f->R.rdi);
	// 		break;
	// 	case SYS_FILESIZE:
	// 		f->R.rax = syscall_filesize (f->R.rdi);
	// 		break;
	// 	case SYS_READ:
	// 		f->R.rax = syscall_read (f->R.rdi, f->R.rsi, f->R.rdx);
	// 		break;
	// 	case SYS_WRITE:
	// 		f->R.rax = syscall_write (f->R.rdi, f->R.rsi, f->R.rdx);
	// 		break;
	// 	case SYS_SEEK:
	// 		syscall_seek (f->R.rdi, f->R.rsi);
	// 		break;
	// 	case SYS_TELL:
	// 		f->R.rax = syscall_tell (f->R.rdi);
	// 		break;
	// 	case SYS_CLOSE:
	// 		syscall_close (f->R.rdi);
	// 		break;
	// 	default:
	// 		thread_exit ();
	// }
	return;
}

//############## implement ################

// void
// syscall_halt (void) {
// 	power_off ();
// }

// void
// syscall_exit (int status) {
// 	struct thread *t = thread_current ();
// 	t->status = status;
// 	printf ("%s: exit(%d)\n", t->name, status);
// 	thread_exit ();
// }

// int
// syscall_exec (const char *cmd_line) {
// 	return process_execute (cmd_line);
// }

// bool
// syscall_create (const char *file, unsigned initial_size) {
// 	if (file == NULL)
// 		syscall_exit (-1);
// 	return filesys_create (file, initial_size);
// }

// bool
// syscall_remove (const char *file) {
// 	if (file == NULL)
// 		syscall_exit (-1);
// 	return filesys_remove (file);
// }

// tid_t
// syscall_fork (struct intr_frame *f) {
// 	return process_fork (f);
// }

// void
// syscall_seek (int fd, unsigned position) {
// 	struct file *file = process_get_file (fd);
// 	if (file == NULL)
// 		syscall_exit (-1);
// 	file_seek (file, position);
// }

// unsigned
// syscall_tell (int fd) {
// 	struct file *file = process_get_file (fd);
// 	if (file == NULL)
// 		syscall_exit (-1);
// 	return file_tell (file);
// }

// int
// syscall_filesize (int fd) {
// 	struct file *file = process_get_file (fd);
// 	if (file == NULL)
// 		syscall_exit (-1);
// 	return file_length (file);
// }

// int
// syscall_wait (tid_t tid) {
// 	return process_wait (tid);
// }

// int
// syscall_open (const char *file) {
// 	if (file == NULL)
// 		syscall_exit (-1);
// 	struct file *f = filesys_open (file);
// 	if (f == NULL)
// 		return -1;
// 	return process_add_file (f);
// }

// int
// syscall_read (int fd, void *buffer, unsigned size) {
// 	if (fd == 0) {
// 		input_getc ();
// 		return 1;
// 	}
// 	struct file *file = process_get_file (fd);
// 	if (file == NULL)
// 		syscall_exit (-1);
// 	return file_read (file, buffer, size);
// }

// int
// syscall_write (int fd, const void *buffer, unsigned size) {
// 	if (fd == 1) {
// 		putbuf (buffer, size);
// 		return size;
// 	}
// 	struct file *file = process_get_file (fd);
// 	if (file == NULL)
// 		syscall_exit (-1);
// 	return file_write (file, buffer, size);
// }
//#########################################