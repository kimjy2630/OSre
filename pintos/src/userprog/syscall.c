#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
////
#include "threads/init.h"
#include "threads/vaddr.h"
#include "lib/kernel/console.h"
#include "lib/user/syscall.h"
////

static void syscall_handler (struct intr_frame *);
////
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
static bool read_validity (const void *uaddr, int size);
static bool write_validity(const void* udst, int size);
static void* get_argument (void *ptr, int pos);


void halt (void);
void exit (int status);
pid_t exec (const char *file);
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
////

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void*
get_argument (void *ptr, int pos)
{
	if (!read_validity(((int*) ptr) + pos, 4)) {
		printf("invalid user pointer read\n");
		thread_current()->exit_status = -1;
		thread_exit();
		return NULL;
	}
	return ((int*) ptr) + pos;
}

static int get_argument_int (void *ptr, int pos)
{
	return *((int*) get_argument(ptr, pos));
}

static int get_argument_ptr (void *ptr, int pos)
{
	return *((void**) get_argument(ptr, pos));
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
	printf("SYSCALL_HANDLER\n");
  /* original code
  printf ("system call!\n");
  thread_exit ();
  */
////
//  int syscall_num = *((int *) (f->esp));
	void *ptr = (void *) f->esp;
	if (!read_validity(ptr, 4)) {
		printf("invalid user pointer read\n");
		thread_exit();
	}
	printf("SYSNUM %d\n", *((int*) ptr));
	switch (*((int*) ptr)) {
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		// int type arg
		exit(get_argument_int(ptr, 1));
		break;
	case SYS_EXEC:
		// char* type arg
		f->eax = exec(get_argument_ptr(ptr, 1));
		break;
	case SYS_WAIT:
		// pid_t type arg
		f->eax = wait(get_argument_int(ptr, 1));
		break;
	case SYS_CREATE:
		// char*, unsigned type arg
		f->eax = create(get_argument_ptr(ptr, 1), get_argument_int(ptr, 2));
		break;
	case SYS_REMOVE:
		// char* type arg
		f->eax = remove(get_argument_ptr(ptr, 1));
		break;
	case SYS_OPEN:
		// char* type arg
		f->eax = open(get_argument_ptr(ptr, 1));
		break;
	case SYS_FILESIZE:
		// int type arg
		f->eax = filesize(get_argument_int(ptr, 1));
		break;
	case SYS_READ:
		// int, void*, unsigned type arg
		f->eax = read(get_argument_int(ptr, 1), get_argument_ptr(ptr, 2),
				get_argument_int(ptr, 3));
		break;
	case SYS_WRITE:
		// int, void*, unsigned type arg
		printf("SYSWRITE %d_%p_%s_%u\n", get_argument_int(ptr, 1),
				get_argument_ptr(ptr, 2), get_argument_ptr(ptr, 2),
				get_argument_int(ptr, 3));
		f->eax = write(get_argument_int(ptr, 1), get_argument_ptr(ptr, 2),
				get_argument_int(ptr, 3));
		printf("\n\nSYSWRITE RET %d\n", f->eax);
		break;
	case SYS_SEEK:
		// int, unsigned type arg
		seek(get_argument_int(ptr, 1), get_argument_int(ptr, 2));
		break;
	case SYS_TELL:
		// unsigned type arg
		f->eax = tell(get_argument_int(ptr, 1));
		break;
	case SYS_CLOSE:
		// unsigned type arg
		close(get_argument_int(ptr, 1));
		break;
	}
//  thread_exit();
////
}

////
void halt (void){
	power_off();
}
void exit (int status){
	printf("SYS_EXIT %d\n", status);
	struct thread *curr = thread_current();
	curr->exit_status = status;
	curr->is_exit = true;
	lock_release(&curr->lock_child);
	thread_exit();
}
pid_t exec (const char *file){
	if (!read_validity(file, strlen(file) + 1)) {
		printf("invalid user pointer read\n");
		thread_current()->exit_status = -1;
		thread_exit();
		return -1;
	}
	return process_execute(file);
}
int wait (pid_t pid){
	//TODO
	return process_wait(pid);
//	return -1;
}
bool create (const char *file, unsigned initial_size){
	if (!read_validity(file, strlen(file) + 1)) {
		printf("invalid user pointer read\n");
		thread_current()->exit_status = -1;
		thread_exit();
		return false;
	}
	return filesys_create(file, initial_size);
}
bool remove (const char *file){
	if (!read_validity(file, strlen(file) + 1)) {
		printf("invalid user pointer read\n");
		thread_current()->exit_status = -1;
		thread_exit();
		return false;
	}

	struct file* f = filesys_open(file);
	if (f == NULL)
		return false;
	file_close(f);
	return filesys_remove(file);
}
int open (const char *file){
	struct file* f;

	f = filesys_open(file);
	if (f == NULL)
		return -1;

	int fd = process_add_file(thread_current(), f, file);

	return fd;
}
int filesize (int fd){
	struct process_fd *pfd = process_get_file(thread_current(), fd);
	if (pfd == NULL)
		return -1;

	int len = file_length(pfd->file);
	return len;
}
int read (int fd, void *buffer, unsigned length){
	if (!read_validity(buffer, length) || !write_validity(buffer, length)) {
		printf("invalid user pointer read\n");
		thread_current()->exit_status = -1;
		thread_exit();
		return -1;
	}

	int result = -1;

	/* Special case for reading from the keyboard */
	if (fd == 0) {
		size_t read_size = 0;
		while (read_size < length)
			buffer[read_size++] = input_getc();

		return read_size;
	}
	struct process_fd *pfd = process_get_file(thread_current(), fd);
	if (pfd == NULL)
		return -1;

	size_t cnt = 0;

	char *tmp_buf = malloc(PGSIZE);
	if (tmp_buf == NULL)
		return -1;

	while (cnt < length) {
		int cur_size = length - cnt;
		if (cur_size > PGSIZE)
			cur_size = PGSIZE;

		char *cur_buff = buffer + cnt;
		int op_result = file_read(pfd->file, tmp_buf, cur_size);
		memcpy(cur_buff, tmp_buf, cur_size);

		cnt += op_result;
		if (op_result != cur_size)
			break;
	}
	free(tmp_buf);
	return cnt;
}
int write (int fd, const void *buffer, unsigned length){
	if (!read_validity(buffer, length)) {
		printf("invalid user pointer read\n");
		thread_current()->exit_status = -1;
		thread_exit();
		return -1;
	}

	if (fd == 1) { // write to console
//			printf("WRITE FD 1\n");
		putbuf(buffer, (size_t) length);
		return length;
	}

	struct process_fd *pfd = process_get_file(thread_current(), fd);
	if (pfd == NULL)
		return 0;

	size_t cnt = 0;

	char *tmp_buf = malloc(PGSIZE);
	if (tmp_buf == NULL)
		return -1;

	while (cnt < length) {
		int cur_size = length - cnt;
		if (cur_size > PGSIZE)
			cur_size = PGSIZE;

		char *cur_buff = buffer + cnt;
		memcpy(tmp_buf, cur_buff, cur_size);
		int op_result = file_write(pfd->file, tmp_buf, cur_size);
		cnt += op_result;
		if (op_result != cur_size)
			break;
	}
	free(tmp_buf);
	return cnt;
}
void seek (int fd, unsigned position){
	struct process_fd *pfd = process_get_file(thread_current(), fd);
	if (pfd == NULL)
		return;
	file_seek(pfd->file, position);
}
unsigned tell (int fd){
	struct process_fd *pfd = process_get_file(thread_current(), fd);
	if (pfd == NULL)
		return -1;
	return file_tell(pfd->file);
}
void close (int fd){
	struct process_fd *pfd = process_get_file(thread_current(), fd);
	if (pfd == NULL)
		return;

	file_close(pfd->file);

	process_remove_file(thread_current(), fd);
}

static int
get_user (const uint8_t *uaddr){
	int result;
	asm ("movl $1f, %0; movzbl %1, %0; 1:"
			: "=&a" (result) : "m" (*uaddr));
	return result;
}
static bool
put_user (uint8_t *udst, uint8_t byte){
	int error_code;
	asm ("movl $1f, %0; movb %b2, %1; 1:"
			: "=&a" (error_code), "=m" (*udst) : "q" (byte));
	return error_code != -1;
}

static bool
read_validity (const void *uaddr, int size){
	int i;
	if (((uint8_t *) uaddr) + size > PHYS_BASE)
		return false;
	for (i = 0; i < size; i++)
	{
		if (get_user(((uint8_t *) uaddr) + i) == -1)
			return false;
	}
	return true;
}

static bool write_validity(const void* udst, int size)
{
	if (((uint8_t*) udst) + size > PHYS_BASE)
		return false;
	int i;
	for (i = 0; i < size; ++i) {
		if (!put_user(((uint8_t*) udst) + i, '\0'))
			return false;
	}
	return true;
}
////
