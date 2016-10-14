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
static bool write_validity (uint8_t *udst, uint8_t byte);
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
	if(!read_validity(ptr, 4)){
		printf("invalid user pointer read\n");
		thread_current()->exit_status = -1;
		thread_exit();
		return NULL;
	}
	return ptr;
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
  if(!read_validity (ptr, 4)){
	  printf("invalid user pointer read\n");
	  thread_exit();
  }
	switch (*((int*) ptr)) {
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		// int type arg
//		if (!read_validity(ptr + 1, 4)) {
//			printf("invalid user pointer read\n");
//			thread_exit();
//		}
//		exit(*((int *) ptr + 1));
		exit(get_argument_int(ptr, 1));
		break;
	case SYS_EXEC:
		// char* type arg
//		if (!read_validity(ptr + 1, 4)) {
//			printf("invalid user pointer read\n");
//			thread_exit();
//		}
//		exec(*((char **) ptr + 1));
		f->eax = exec(get_argument_ptr(ptr, 1));
		break;
	case SYS_WAIT:
		// pid_t type arg
//		if (!read_validity(ptr + 1, 4)) {
//			printf("invalid user pointer read\n");
//			thread_exit();
//		}
//		wait(*((pid_t *) ptr + 1));
		f->eax = wait(get_argument_int(ptr, 1));
		break;
	case SYS_CREATE:
		// char*, unsigned type arg
//		if (!read_validity(ptr + 1, 8)) {
//			printf("invalid user pointer read\n");
//			thread_exit();
//		}
//		create(*((char **) ptr + 1), *(unsigned *) ptr + 2);
		f->eax = create(get_argument_ptr(ptr, 1), get_argument_int(ptr, 2));
		break;
	case SYS_REMOVE:
		// char* type arg
//		if (!read_validity(ptr + 1, 4)) {
//			printf("invalid user pointer read\n");
//			thread_exit();
//		}
//		remove(*((char **) ptr + 1));
		f->eax = remove(get_argument_ptr(ptr, 1));
		break;
	case SYS_OPEN:
		// char* type arg
//		if (!read_validity(ptr + 1, 4)) {
//			printf("invalid user pointer read\n");
//			thread_exit();
//		}
//		open(*((char **) ptr + 1));
		f->eax = open(get_argument_ptr(ptr, 1));
		break;
	case SYS_FILESIZE:
		// int type arg
//		filesize(0);
		f->eax = filesize(get_argument_int(ptr, 1));
		break;
	case SYS_READ:
		// int, void*, unsigned type arg
//		read(0, NULL, 0);
		f->eax = read(get_argument_int(ptr, 1), get_argument_ptr(ptr, 2),
				get_argument_int(ptr, 3));
		break;
	case SYS_WRITE:
		// int, void*, unsigned type arg
//		write(0, NULL, 0);
		f->eax = write(get_argument_int(ptr, 1), get_argument_ptr(ptr, 2),
				get_argument_int(ptr, 3));
		break;
	case SYS_SEEK:
		// int, unsigned type arg
//		seek(0, 0);
		seek(get_argument_int(ptr, 1), get_argument_int(ptr, 2));
		break;
	case SYS_TELL:
		// unsigned type arg
//		tell(0);
		f->eax = tell(get_argument_int(ptr, 1));
		break;
	case SYS_CLOSE:
		// unsigned type arg
//		close(0);
		close(get_argument_int(ptr, 1));
		break;
	}
  thread_exit();
////
}

////
void halt (void){
	power_off();
}
void exit (int status){
	struct thread *curr = thread_current();
	curr->exit_status = status;
	curr->is_exit = true;
	thread_exit();
}
pid_t exec (const char *file){
	return -1;
}
int wait (pid_t pid){
	//TODO
	return process_wait(pid);
//	return -1;
}
bool create (const char *file, unsigned initial_size){
	return false;
}
bool remove (const char *file){
	return false;
}
int open (const char *file){
	return -1;
}
int filesize (int fd){
	return -1;
}
int read (int fd, void *buffer, unsigned length){
	return -1;
}
int write (int fd, const void *buffer, unsigned length){
	if(fd == 1){ // write to console
		putbuf(buffer, (size_t) length);
		return length;
	}
	return -1;
}
void seek (int fd, unsigned position){

}
unsigned tell (int fd){
	return 0;
}
void close (int fd){

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

static bool
write_validity (uint8_t *udst, uint8_t byte){
	return (udst < PHYS_BASE) && put_user (udst, byte);
}
////
