#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#define FDT_LIMIT (1 << 9)

#include "threads/thread.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

// implement for argument passing
void argument_tokenize (char *fn, int *argc, char *argv[]);
void argument_insert (int argc, char *argv[], struct intr_frame *if_);

// implement for fork, wait
struct thread* get_child_process (tid_t child_pid);

#endif /* userprog/process.h */

