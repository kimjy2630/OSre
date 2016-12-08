#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

typedef int pid_t;

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);


struct process_file {
	struct list_elem elem;
	int fd;
	struct file *file;
#ifdef FILESYS
	bool is_dir;
#endif
};

struct arg_success {
	char *fn_copy;
	bool success;
	struct file *f;
	struct semaphore loading;
	struct thread *parent;
};

struct process_status {
	struct list_elem elem;
	tid_t tid;
	struct thread* t;
	int exit_status;
};

struct process_file*
get_process_file_from_fd(struct thread* t, int fd);
int
add_process_file(struct thread* t, struct file* file, const char* filename);
void
remove_process_file_from_fd(struct thread* t, int fd);

#endif /* userprog/process.h */
