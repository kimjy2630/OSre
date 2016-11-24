#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "threads/vaddr.h"
#include "lib/kernel/console.h"
#include <string.h>
#include "threads/synch.h"
#include "lib/kernel/hash.h"
//#include "userprog/pagedir.h"
#include "filesys/file.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/mmap.h"

static void syscall_handler(struct intr_frame *);

void* esp;

struct lock lock_file;

void syscall_init(void) {
	intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");

	lock_init(&lock_file);
}

static void*
get_argument(void *ptr, int pos) {
	if (!read_validity(((int*) ptr) + pos, 4)) {
		exit(-1);
		return NULL;
	}
	return ((int*) ptr) + pos;
}

static int get_argument_int(void *ptr, int pos) {
	return *((int*) get_argument(ptr, pos));
}

static int get_argument_ptr(void *ptr, int pos) {
	return *((void**) get_argument(ptr, pos));
}

static void
syscall_handler (struct intr_frame *f UNUSED)
{
	void *ptr = (void *) f->esp;
	esp = f->esp;
#ifdef VM
	thread_current()->esp = f->esp;
#endif
//	printf("read validity:%p\n", ptr);
	if (!read_validity(ptr, 4)){
//		printf("hahaha\n");
		exit(-1);
	}
//	printf("I'm valid!\n");
	switch (*((int*) ptr)) {
		case SYS_HALT:
		halt();
		break;
		case SYS_EXIT:
		exit(get_argument_int(ptr, 1));
		break;
		case SYS_EXEC:
		f->eax = exec(get_argument_ptr(ptr, 1));
		break;
		case SYS_WAIT:
		f->eax = wait(get_argument_int(ptr, 1));
		break;
		case SYS_CREATE:
		f->eax = create(get_argument_ptr(ptr, 1), get_argument_int(ptr, 2));
		break;
		case SYS_REMOVE:
		f->eax = remove(get_argument_ptr(ptr, 1));
		break;
		case SYS_OPEN:
		f->eax = open(get_argument_ptr(ptr, 1));
		break;
		case SYS_FILESIZE:
		f->eax = filesize(get_argument_int(ptr, 1));
		break;
		case SYS_READ:
		f->eax = read(get_argument_int(ptr, 1), get_argument_ptr(ptr, 2),
				get_argument_int(ptr, 3));
		break;
		case SYS_WRITE:
		f->eax = write(get_argument_int(ptr, 1), get_argument_ptr(ptr, 2),
				get_argument_int(ptr, 3));
		break;
		case SYS_SEEK:
		seek(get_argument_int(ptr, 1), get_argument_int(ptr, 2));
		break;
		case SYS_TELL:
		f->eax = tell(get_argument_int(ptr, 1));
		break;
		case SYS_CLOSE:
		close(get_argument_int(ptr, 1));
		break;
		case SYS_MMAP:
		mmap(get_argument_int(ptr, 1), get_argument_ptr(ptr,2));
		break;
		case SYS_MUNMAP:
		munmap(get_argument_int(ptr,1));
		break;
	}
}

void halt(void) {
	power_off();
}

void exit(int status) {
	if(lock_held_by_current_thread(&lock_file))
		lock_release(&lock_file);
	struct thread *curr = thread_current();
	curr->exit_status = status;
	if (curr->ps != NULL)
		curr->ps->exit_status = status;
//#ifdef VM
////	printf("clear supp page table\n");
//	supp_page_table_destroy(&curr->supp_page_table);
//#endif
	curr->is_exit = true;
	thread_exit();
}

pid_t exec(const char *file) {
	if (!read_validity(file, strlen(file) + 1)) {
		exit(-1);
		return -1;
	}
	return process_execute(file);
}

int wait(pid_t pid) {
	return process_wait(pid);
}

bool create(const char *file, unsigned initial_size) {
	if (!read_validity(file, strlen(file) + 1)) {
		exit(-1);
		return false;
	}
	return filesys_create(file, initial_size);
}
bool remove(const char *file) {
	if (!read_validity(file, strlen(file) + 1)) {
		exit(-1);
		return false;
	}

	struct file* f = filesys_open(file);
	if (f == NULL)
		return false;
	file_close(f);
	return filesys_remove(file);
}
int open(const char *file) {
	if (!read_validity(file, strlen(file) + 1)) {
//		printf("sys open read validity error\n");
		exit(-1);
		return -1;
	}

	lock_acquire(&lock_file);
	struct file* f;
	f = filesys_open(file);
	if (f == NULL) {
		lock_release(&lock_file);
		return -1;
	}
	int fd = add_process_file(thread_current(), f, file);
	lock_release(&lock_file);
	return fd;
}
int filesize(int fd) {
	lock_acquire(&lock_file);
	struct process_file *pf = get_process_file_from_fd(thread_current(), fd);
	if (pf == NULL) {
		lock_release(&lock_file);
		return -1;
	}

	int len = file_length(pf->file);
	lock_release(&lock_file);
	return len;
}
int read(int fd, void *buffer, unsigned length) {
	if (!read_validity(buffer, length) || !write_validity(buffer, length)) {
//		printf("read not valid\n");
		exit(-1);
		return -1;
	}

	int result = -1;

	if (fd == 0) {
		size_t read_size = 0;
		char* buf = (char*) buffer;
		while (read_size < length)
			buf[read_size++] = input_getc();

		return read_size;
	}


	struct process_file *pf = get_process_file_from_fd(thread_current(), fd);
	if (pf == NULL){
		return -1;
	}
	/*
	size_t cnt = 0;

	char *tmp_buf = malloc(PGSIZE);
	if (tmp_buf == NULL)
		return -1;

	while (cnt < length) {
		int cur_size = length - cnt;
		if (cur_size > PGSIZE)
			cur_size = PGSIZE;

		char *cur_buff = buffer + cnt;
		int op_result = file_read(pf->file, tmp_buf, cur_size);
		memcpy(cur_buff, tmp_buf, cur_size);

		cnt += op_result;
		if (op_result != cur_size)
			break;
	}
	free(tmp_buf);
	return cnt;
	*/
//	/*
	void* tmp_buf = buffer;
	unsigned rest = length;
	int cnt = 0;
	while (rest > 0) {
		size_t ofs = tmp_buf - pg_round_down(tmp_buf);
		struct supp_page_entry spe_tmp;
		spe_tmp.uaddr = tmp_buf - ofs;
		struct hash_elem* he = hash_find(&thread_current()->supp_page_table,
				&spe_tmp.elem);
		struct supp_page_entry* spe = NULL;
		if (he == NULL) {
			if (tmp_buf >= (esp - 32)
					&& (PHYS_BASE - pg_round_down(tmp_buf)) <= (1 << 23))
				spe = stack_grow(tmp_buf - ofs);
			else {
				exit(-1);
				return -1;
			}
		} else
			spe = hash_entry(he, struct supp_page_entry, elem);

//		ASSERT(spe != NULL);
//		ASSERT(tmp_buf != NULL);
//		lock_acquire(&spe->lock); //////
		spe->fe->finned = true;
		size_t read_bytes = ofs + rest > PGSIZE ? PGSIZE - ofs : rest;

		lock_acquire(&lock_file);
		cnt += file_read(pf->file, tmp_buf, read_bytes);
		lock_release(&lock_file);

		rest -= read_bytes;
		tmp_buf += read_bytes;

		spe->fe->finned = false;
//		lock_release(&spe->lock); //////
	}
	return cnt;
//	*/
}

int write(int fd, const void *buffer, unsigned length) {
//	printf("sys write\n");
	if (!read_validity(buffer, length)) {
//		printf("write not valid\n");
		exit(-1);
		return -1;
	}

	if (fd == 1) { // write to console
//		printf("write to console\n");
		putbuf(buffer, (size_t) length);
		return length;
	}

	struct process_file *pf = get_process_file_from_fd(thread_current(), fd);
	if (pf == NULL){
//		printf("write null file!\n");
		return 0;
	}
	/*
	size_t cnt = 0;

	char *tmp_buf = malloc(PGSIZE);
	if (tmp_buf == NULL)

	while (cnt < length) {
		int cur_size = length - cnt;
		if (cur_size > PGSIZE)
			cur_size = PGSIZE;

		char *cur_buff = buffer + cnt;
		memcpy(tmp_buf, cur_buff, cur_size);
		int op_result = file_write(pf->file, tmp_buf, cur_size);
		cnt += op_result;
		if (op_result != cur_size)
			break;
	}
	free(tmp_buf);
	return cnt;
	*/
//	/*
	unsigned rest = length;
	void *tmp_buf = (void *) buffer;
	int cnt = 0;
	while (rest > 0) {
		size_t ofs = tmp_buf - pg_round_down(tmp_buf);
		struct supp_page_entry spe_tmp;
		spe_tmp.uaddr = tmp_buf - ofs;
		struct hash_elem* he = hash_find(&thread_current()->supp_page_table, &spe_tmp.elem);
		struct supp_page_entry* spe;
		if (he == NULL) {
			if (tmp_buf >= (esp - 32)
					&& (PHYS_BASE - pg_round_down(tmp_buf)) <= (1 << 23))
				spe = stack_grow(tmp_buf - ofs);
			else {
				exit(-1);
				return -1;
			}
		} else
			spe = hash_entry(he, struct supp_page_entry, elem);

//		ASSERT(spe != NULL);
//		ASSERT(tmp_buf !=NULL);

//		lock_acquire(&spe->lock); //////
		spe->fe->finned = true;
		size_t write_bytes = ofs + rest > PGSIZE ? PGSIZE - ofs : rest;

		lock_acquire(&lock_file);
		cnt += file_write(pf->file, tmp_buf, write_bytes);
		lock_release(&lock_file);

		rest -= write_bytes;
		tmp_buf += write_bytes;

		spe->fe->finned = false;
//		lock_release(&spe->lock);//////
	}
	return cnt;
//	*/
}

void seek(int fd, unsigned position) {
	struct process_file *pf = get_process_file_from_fd(thread_current(), fd);
	if (pf == NULL)
		return;
	file_seek(pf->file, position);
}
unsigned tell(int fd) {
	struct process_file *pf = get_process_file_from_fd(thread_current(), fd);
	if (pf == NULL)
		return -1;
	return file_tell(pf->file);
}
void close(int fd) {
	struct process_file *pf = get_process_file_from_fd(thread_current(), fd);
	if (pf == NULL)
		return;

	if (pf->file != NULL)
		file_close(pf->file);
	pf->file = NULL;
	remove_process_file_from_fd(thread_current(), fd);
}

mapid_t mmap(int fd, uint8_t *uaddr){
	if(uaddr > PHYS_BASE)
		exit(-1);

	if(uaddr == 0 || pg_ofs(uaddr) != 0 || fd == 0 || fd == 1)
		return -1;

	struct process_file *pf = get_process_file_from_fd(thread_current(), fd);
	if(pf == NULL)
		return -1;

	lock_acquire(&lock_file);
	struct file *file = file_reopen(pf->file);
	off_t length = file_length(file);
	lock_release(&lock_file);

	int num_page = length / PGSIZE;
	if(length % PGSIZE != 0)
		num_page++;

	int i;
	for(i=0; i<num_page; i++){
		struct supp_page_entry spe_tmp;
		spe_tmp.uaddr = uaddr + i * PGSIZE;
		struct hash_elem* he = hash_find(&thread_current()->supp_page_table, &spe_tmp.elem);
		if(he != NULL)
			return -1;
	}

	struct mmapping *mmap = add_mmap(thread_current(), fd, uaddr);

	unsigned rest = file_length;
	uint8_t *tmp_addr = uaddr;
	size_t ofs = 0;

	while(rest>0){
		struct supp_page_entry *spe = supp_page_add(tmp_addr, true);
		size_t read_bytes = rest > PGSIZE ? PGSIZE : rest;
		spe->fe->finned = true;
		spe->type = MMAP;
		spe->mmap = mmap;
		spe->mmap_ofs = ofs;
		spe->mmap_page_read_bytes = read_bytes;
		spe->fe->finned = false;
		rest -= read_bytes;
		ofs += read_bytes;
		tmp_addr += read_bytes;
	}

	return mmap->mapid;
}

void munmap(mapid_t mapid){
	struct thread *t = thread_current();
	struct mmapping *mmap = get_mmap_from_mapid(t, mapid);
	if(mmap == NULL)
		return;

	lock_acquire(&lock_file);
	off_t length = file_length(mmap->file);
	lock_release(&lock_file);

	int num_page = length / PGSIZE;
	if(length % PGSIZE != 0)
		num_page++;

	uint8_t *uaddr = mmap->uaddr;
	int i;
	for(i=0; i<num_page; i++){
		struct supp_page_entry spe_tmp;
		spe_tmp.uaddr = uaddr;
		struct hash_elem *he = hash_find(&t->supp_page_table, &spe_tmp.elem);
		ASSERT(he != NULL);

		struct supp_page_table *spe = hash_entry(he, struct supp_page_entry, elem);
		if(spe->type == MEMORY){
			struct mmapping *mmap = spe->mmap;
			uint8_t *kaddr = spe->kaddr;
			if(pagedir_is_dirty(t->pagedir, kaddr)){
				struct file *file = mmap->file;
				lock_acquire(&lock_file);
				file_seek(file, spe->mmap_ofs);
				file_write(file, kaddr, spe->mmap_page_read_bytes);
				lock_release(&lock_file);
			}
			frame_free_fe(spe->fe);
			pagedir_clear_page(t->pagedir, uaddr);
		}
		hash_delete(&t->supp_page_table, he);
		free(spe);
		uaddr += PGSIZE;
	}
}

int get_user(const uint8_t *uaddr) {
	int result;
	asm ("movl $1f, %0; movzbl %1, %0; 1:"
			: "=&a" (result) : "m" (*uaddr));
	return result;
}
bool put_user(uint8_t *udst, uint8_t byte) {
	int error_code;
	asm ("movl $1f, %0; movb %b2, %1; 1:"
			: "=&a" (error_code), "=m" (*udst) : "q" (byte));
	return error_code != -1;
}

bool read_validity(const void *uaddr, int size) {
	int i;
	if (((uint8_t *) uaddr) + size > PHYS_BASE)
		return false;
	for (i = 0; i < size; i++) {
		if (get_user(((uint8_t *) uaddr) + i) == -1)
			return false;
	}
	return true;
}

bool write_validity(const void* udst, int size) {
	if (((uint8_t*) udst) + size > PHYS_BASE)
		return false;
	int i;
	for (i = 0; i < size; ++i) {
		if (!put_user(((uint8_t*) udst) + i, '\0'))
			return false;
	}
	return true;
}
