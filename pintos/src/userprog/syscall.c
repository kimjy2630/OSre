#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
////
#include "threads/init.h"
////

static void syscall_handler (struct intr_frame *);
////
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
static bool read_validity (const uint8_t *uaddr);
static bool write_validity (uint8_t *udst, uint8_t byte);
////

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  /* original code
  printf ("system call!\n");
  thread_exit ();
  */
////
//  int syscall_num = *((int *) (f->esp));
  int *ptr = (int *) f->esp;
  if(!read_validity (uint8_t *) ptr){
	  printf("invalid user pointer read\n");
	  thread_exit();
  }
  switch (*ptr){
    case SYS_HALT:
    	halt();
    	break;
    case SYS_EXIT:
    	break;
  }
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

}
int wait (pid_t pid){

}
bool create (const char *file, unsigned initial_size){

}
bool remove (const char *file){

}
int open (const char *file){

}
int filesize (int fd){

}
int read (int fd, void *buffer, unsigned length){

}
int write (int fd, const void *buffer, unsigned length){

}
void seek (int fd, unsigned position){

}
unsigned tell (int fd){

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
read_validity (const uint8_t *uaddr){
	return (uaddr < PHYS_BASE) && (get_user(uaddr) != -1);
}
static bool
write_validity (uint8_t *udst, uint8_t byte){
	return (udst < PHYS_BASE) && put_user (udst, byte);
}
////
