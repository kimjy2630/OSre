#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

typedef int pit_t;

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct process_file {
	struct list_elem elem;
	int fd;
	struct file *file;
	const char *filename;
};

static struct process_file*
get_process_file_from_fd(struct thread* t, int fd);
static int
add_process_file(struct thread* t, struct file* file, const char* filename);
static void
remove_process_file_from_fd(struct thread* t, int fd);

#endif /* userprog/process.h */
