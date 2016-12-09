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
#include "filesys/file.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/mmap.h"
#ifdef FILESYS
#include "filesys/directory.h"
#endif

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

static void syscall_handler(struct intr_frame *f UNUSED) {
	void *ptr = (void *) f->esp;
	esp = f->esp;
#ifdef VM
	thread_current()->esp = f->esp;
#endif
	if (!read_validity(ptr, 4)) {
		exit(-1);
	}
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
		f->eax = read(get_argument_int(ptr, 1), get_argument_ptr(ptr, 2), get_argument_int(ptr, 3));
		break;
	case SYS_WRITE:
		f->eax = write(get_argument_int(ptr, 1), get_argument_ptr(ptr, 2), get_argument_int(ptr, 3));
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
#ifdef VM
		case SYS_MMAP:
		f->eax = mmap(get_argument_int(ptr, 1), get_argument_ptr(ptr,2));
		break;
		case SYS_MUNMAP:
		munmap(get_argument_int(ptr,1));
		break;
#endif
#ifdef FILESYS
		case SYS_CHDIR:
		f->eax = chdir(get_argument_ptr(ptr, 1));
		break;
		case SYS_MKDIR:
		f->eax = mkdir(get_argument_ptr(ptr, 1));
		break;
		case SYS_READDIR:
		f->eax = readdir(get_argument_int(ptr, 1), get_argument_ptr(ptr, 2));
		break;
		case SYS_ISDIR:
		f->eax = isdir(get_argument_int(ptr, 1));
		break;
		case SYS_INUMBER:
		f->eax = inumber(get_argument_int(ptr, 1));
		break;
#endif
	}
}

void halt(void) {
	power_off();
}

void exit(int status) {
	if (lock_held_by_current_thread(&lock_file))
		lock_release(&lock_file);
	struct thread *curr = thread_current();
	curr->exit_status = status;
	if (curr->ps != NULL)
		curr->ps->exit_status = status;
	curr->is_exit = true;
	printf("%s: exit(%d)\n", thread_current()->name, thread_current()->exit_status);
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
	return filesys_create(file, initial_size, false);
}
bool remove(const char *file) {
	if (!read_validity(file, strlen(file) + 1)) {
		exit(-1);
		return false;
	}
//	struct file* f = filesys_open(file);
//	if (f == NULL){
//		printf("remove: filesys_open(file) is NULL. file [%s]\n", file);
//		return false;
//	}
//	file_close(f);
	return filesys_remove(file);
}
int open(const char *file) {
	if (!read_validity(file, strlen(file) + 1)) {
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
	struct process_file *pf = add_process_file(thread_current(), f, file);
	int fd = pf->fd;
#ifdef FILESYS
	struct inode *inode = file_get_inode(f);
	if(inode_is_dir(inode)) {
		pf->dir = dir_open(inode_reopen(inode));
	}
#endif
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
	if (pf == NULL) {
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
#ifdef VM
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

		ASSERT(spe->uaddr <= PHYS_BASE);

		spe->fe->finned = true;
		size_t read_bytes = ofs + rest > PGSIZE ? PGSIZE - ofs : rest;

		lock_acquire(&lock_file);
		cnt += file_read(pf->file, tmp_buf, read_bytes);
		lock_release(&lock_file);

		rest -= read_bytes;
		tmp_buf += read_bytes;

		spe->fe->finned = false;
	}
	return cnt;
#else
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
#endif
}

int write(int fd, const void *buffer, unsigned length) {
	if (!read_validity(buffer, length)) {
		exit(-1);
		return -1;
	}

	if (fd == 1) {
		putbuf(buffer, (size_t) length);
		return length;
	}

	struct process_file *pf = get_process_file_from_fd(thread_current(), fd);
	if (pf == NULL) {
		return 0;
	}
#ifdef FILESYS
	if(pf->dir != NULL)
	return -1;
#endif
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
	 memcpy(tmp_buf, cur_buff, cur_size);
	 int op_result = file_write(pf->file, tmp_buf, cur_size);
	 cnt += op_result;
	 if (op_result != cur_size)
	 break;
	 }
	 free(tmp_buf);
	 return cnt;
	 */
#ifdef VM
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

		ASSERT(spe->uaddr <= PHYS_BASE);
		spe->fe->finned = true;
		size_t write_bytes = ofs + rest > PGSIZE ? PGSIZE - ofs : rest;

		lock_acquire(&lock_file);
		cnt += file_write(pf->file, tmp_buf, write_bytes);
		lock_release(&lock_file);

		rest -= write_bytes;
		tmp_buf += write_bytes;

		spe->fe->finned = false;
	}
	return cnt;
#else
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
		int op_result = file_write(pf->file, tmp_buf, cur_size);
		cnt += op_result;
		if (op_result != cur_size)
			break;
	}
	free(tmp_buf);
	return cnt;
#endif
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

	if (pf->file != NULL){
#ifdef FILESYS
		if(pf->dir != NULL)
			dir_close(pf->dir);
#endif
		file_close(pf->file);
	}
	pf->file = NULL;
	remove_process_file_from_fd(thread_current(), fd);
}

#ifdef VM
mapid_t mmap(int fd, uint8_t *uaddr) {
	if(uaddr > PHYS_BASE) {
		exit(-1);
	}
	if(uaddr == 0 || pg_ofs(uaddr) != 0 || fd == 0 || fd == 1) {
		return -1;
	}
	struct process_file *pf = get_process_file_from_fd(thread_current(), fd);
	if(pf == NULL) {
		return -1;
	}
	off_t length = filesize(pf->fd);
	if(length == 0) {
		return -1;
	}
	lock_acquire(&lock_file);
	struct file *file = file_reopen(pf->file);
	lock_release(&lock_file);
	int num_page = length / PGSIZE;
	if(length % PGSIZE != 0)
	num_page++;
	int i;
	for(i=0; i<num_page; i++) {
		struct supp_page_entry spe_tmp;
		spe_tmp.uaddr = uaddr + i * PGSIZE;
		struct hash_elem* he = hash_find(&thread_current()->supp_page_table, &spe_tmp.elem);
		if(he != NULL) {
			file_close(file);
			return -1;
		}
	}
	struct mmapping *mmap = add_mmap(thread_current(), file, uaddr);

	unsigned rest = length;
	uint8_t *tmp_addr = uaddr;
	size_t mmap_ofs = 0;
	while(rest>0) {
		struct supp_page_entry *spe = supp_page_add(tmp_addr, true);
		size_t read_bytes = rest > PGSIZE ? PGSIZE : rest;
		spe->type = MMAP;
		spe->mmap = mmap;
		spe->mmap_ofs = mmap_ofs;
		spe->mmap_page_read_bytes = read_bytes;
		rest -= read_bytes;
		mmap_ofs += read_bytes;
		tmp_addr += read_bytes;

		ASSERT(spe->uaddr <= PHYS_BASE);
	}
	mapid_t ret_mapid = mmap->mapid;
	return ret_mapid;
}

void munmap(mapid_t mapid) {
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
	for(i=0; i<num_page; i++) {
		struct supp_page_entry spe_tmp;
		spe_tmp.uaddr = uaddr;
		struct hash_elem *he = hash_find(&t->supp_page_table, &spe_tmp.elem);
		ASSERT(he != NULL);

		struct supp_page_entry *spe = hash_entry(he, struct supp_page_entry, elem);
		if(spe->type == MEM_MMAP) {
			uint8_t *kaddr = spe->kaddr;
			if(pagedir_is_dirty(spe->t->pagedir, uaddr)) {
				struct file *file = spe->mmap->file;
				lock_acquire(&lock_file);
				file_write_at(file, kaddr, spe->mmap_page_read_bytes, spe->mmap_ofs);
				lock_release(&lock_file);
			}
			pagedir_clear_page(spe->t->pagedir, spe->uaddr);
			frame_free_fe(spe->fe);
		}
		pagedir_clear_page(t->pagedir, uaddr);
		lock_page_acquire();
		hash_delete(&t->supp_page_table, he);
		lock_page_release();
		ASSERT(spe->uaddr <= PHYS_BASE);
		free(spe);
		uaddr += PGSIZE;
	}
}
#endif

#ifdef FILESYS
bool chdir(const char* dir) {
	if (!read_validity(dir, strlen(dir) + 1)) {
		exit(-1);
		return false;
	}
	struct dir *dir_target = dir_open_path(dir);
	if(dir_target == NULL)
	return false;

	struct dir *curr_dir = thread_current()->curr_dir;
	if(curr_dir != NULL)
		dir_close(curr_dir);
	thread_current()->curr_dir = dir_target;
//	printf("chdir: change dir to [%s]\n", dir);
	return true;
}
bool mkdir(const char* dir) {
	if (!read_validity(dir, strlen(dir) + 1)) {
		exit(-1);
		return false;
	}
	return filesys_create(dir, 0, true);
}
bool readdir(int fd, const char* name) {
	struct process_file *pf = get_process_file_from_fd(thread_current(), fd);
	if(pf == NULL)
		return false;

	struct inode *inode = file_get_inode(pf->file);
	if(inode == NULL || !inode_is_dir(inode))
		return false;

	struct dir *dir = pf->dir;
	ASSERT(dir != NULL);
	bool success = dir_readdir(dir, name);
	if(success)
		printf("readdir: name [%s]\n", name);
	else
		printf("readdir: fail\n");
	return success;
}
bool isdir(int fd) {
	struct process_file *pf = get_process_file_from_fd(thread_current(), fd);
	if(pf == NULL)
		return false;

	struct inode *inode = file_get_inode(pf->file);
	return inode_is_dir(inode);
}
int inumber(int fd) {
	struct process_file *pf = get_process_file_from_fd(thread_current(), fd);
	struct inode *inode = file_get_inode(pf->file);
	return inode_get_inumber(inode);
}
#endif

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
