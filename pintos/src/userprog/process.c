#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static thread_func start_process NO_RETURN;
static bool load(const char *cmdline, void (**eip)(void), void **esp);
/////
int get_argc(const char* file_name);
bool push_argument(char ** file, int argc, void ** esp);



/* Starts a new thread running a user program loaded from
 FILENAME.  The new thread may be scheduled (and may even exit)
 before process_execute() returns.  Returns the new process's
 thread id, or TID_ERROR if the thread cannot be created. */
tid_t process_execute(const char *file_name) {
	tid_t tid;

	//TODO
	struct arg_success *as = malloc(sizeof(struct arg_success));
	if (as == NULL)
		return TID_ERROR;
	memset (as, 0, sizeof (struct arg_success));
	sema_init(&as->loading, 0);

	/* Make a copy of FILE_NAME.
	 Otherwise there's a race between the caller and load(). */

	as->fn_copy = palloc_get_page(0);
	if(as->fn_copy==NULL)
	{
		free(as);
		return TID_ERROR;
	}
	strlcpy(as->fn_copy, file_name, PGSIZE);


	/* Create a new thread to execute FILE_NAME. */

	tid = thread_create(file_name, PRI_DEFAULT, start_process, as);
	sema_down(&as->loading);
	if (!as->success)
		tid = -1;
	palloc_free_page(as->fn_copy);
	//TODO
	free(as);

	return tid;
}

/* A thread function that loads a user process and makes it start
 running. */
static void start_process(void *f_name) {
	char *file_name = ((struct arg_success *) f_name)->fn_copy;
	struct intr_frame if_;
	bool success;

	thread_current()->user_thread = true;

	/* Initialize interrupt frame and load executable. */
	memset(&if_, 0, sizeof if_);
	if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
	if_.cs = SEL_UCSEG;
	if_.eflags = FLAG_IF | FLAG_MBS;
	success = load(file_name, &if_.eip, &if_.esp);

	((struct arg_success *) f_name)->success = success;
	sema_up(&((struct arg_success *) f_name)->loading);

	/* If load failed, quit. */
	if (!success) {
		exit(-1);
	}

	/* Start the user process by simulating a return from an
	 interrupt, implemented by intr_exit (in
	 threads/intr-stubs.S).  Because intr_exit takes all of its
	 arguments on the stack in the form of a `struct intr_frame',
	 we just point the stack pointer (%esp) to our stack frame
	 and jump to it. */
	asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
	NOT_REACHED();
}

/* Waits for thread TID to die and returns its exit status.  If
 it was terminated by the kernel (i.e. killed due to an
 exception), returns -1.  If TID is invalid or if it was not a
 child of the calling process, or if process_wait() has already
 been successfully called for the given TID, returns -1
 immediately, without waiting.

 This function will be implemented in problem 2-2.  For now, it
 does nothing. */
int process_wait(tid_t child_tid) {
	int i = 0;
	struct list_elem *e;
	struct thread *t = thread_current();
	struct list *list_ps = &t->list_ps;
	struct process_status* child;

	enum intr_level old_level;

	bool flag = false;
	for (e = list_begin(list_ps); e != list_end(list_ps); e = list_next(e)) {
		child = list_entry(e, struct process_status, elem);
		if (child->tid == child_tid) {
			flag = true;
			list_remove(e);
			break;
		}
	}
	if (flag) {

		while (child->t->user_thread && !child->t->is_exit) {
//			barrier();
			old_level = intr_disable();
			thread_block();
//			intr_set_level(old_level);
		}
		int status = child->exit_status;
		//TODO
		if (child->t != NULL){
			child->t->ps = NULL;
			child->t->parent = NULL;
		}
		//TODO
		free(child);

		return status;
	}
	return -1;
}

/* Free the current process's resources. */
void process_exit(void) {
	enum intr_level old = intr_disable();
	struct thread *curr = thread_current();
	int tid = curr->tid;
	uint32_t *pd;

	/* Destroy the current process's page directory and switch back
	 to the kernel-only page directory. */
	pd = curr->pagedir;
	if (pd != NULL) {
		/* Correct ordering here is crucial.  We must set
		 cur->pagedir to NULL before switching page directories,
		 so that a timer interrupt can't switch back to the
		 process page directory.  We must activate the base page
		 directory before destroying the process's page
		 directory, or our active page directory will be one
		 that's been freed (and cleared). */
		curr->pagedir = NULL;
		pagedir_activate(NULL);
		pagedir_destroy(pd);
	}

	printf("%s: exit(%d)\n", thread_current()->name, thread_current()->exit_status);

	if (curr->f != NULL) {
		file_close(curr->f);
		curr->f = NULL;
	}
	struct list* list_ps = &curr->list_ps;
	while (!list_empty(list_ps)) {
		struct process_status* ps = list_entry(list_pop_front(list_ps),
				struct process_status, elem);
		if (ps != NULL) {
			if (ps->t != NULL) {
				ps->t->parent = NULL;
				if (!ps->t->is_exit)
					process_wait(ps->tid);
				ps->t->ps = NULL;
			}
			free(ps);
		}
	}

	struct list* list_pf = &curr->list_pf;
	while (!list_empty(list_pf)) {
		struct process_file* pf = list_entry(list_pop_front(list_pf),
				struct process_file, elem);
		if (pf != NULL) {
			if (pf->file != NULL)
				file_close(pf->file);
			pf->file = NULL;
			free(pf);
		}
	}

	intr_set_level(old);
}

/* Sets up the CPU for running user code in the current
 thread.
 This function is called on every context switch. */
void process_activate(void) {
	struct thread *t = thread_current();
	int tid = t->tid;
	enum intr_level old = intr_disable();
	intr_set_level(old);

	/* Activate thread's page tables. */
	pagedir_activate(t->pagedir);

	/* Set thread's kernel stack for use in processing
	 interrupts. */
	tss_update();
}

/* We load ELF binaries.  The following definitions are taken
 from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr {
	unsigned char e_ident[16];
	Elf32_Half e_type;
	Elf32_Half e_machine;
	Elf32_Word e_version;
	Elf32_Addr e_entry;
	Elf32_Off e_phoff;
	Elf32_Off e_shoff;
	Elf32_Word e_flags;
	Elf32_Half e_ehsize;
	Elf32_Half e_phentsize;
	Elf32_Half e_phnum;
	Elf32_Half e_shentsize;
	Elf32_Half e_shnum;
	Elf32_Half e_shstrndx;
};

/* Program header.  See [ELF1] 2-2 to 2-4.
 There are e_phnum of these, starting at file offset e_phoff
 (see [ELF1] 1-6). */
struct Elf32_Phdr {
	Elf32_Word p_type;
	Elf32_Off p_offset;
	Elf32_Addr p_vaddr;
	Elf32_Addr p_paddr;
	Elf32_Word p_filesz;
	Elf32_Word p_memsz;
	Elf32_Word p_flags;
	Elf32_Word p_align;
};

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack(void **esp);
static bool validate_segment(const struct Elf32_Phdr *, struct file *);
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
 Stores the executable's entry point into *EIP
 and its initial stack pointer into *ESP.
 Returns true if successful, false otherwise. */
bool load(const char *file_name, void (**eip)(void), void **esp) {
	struct thread *t = thread_current();
	struct Elf32_Ehdr ehdr;
	struct file *file = NULL;
	off_t file_ofs;
	bool success = false;
	int i;

	/* Allocate and activate page directory. */
	t->pagedir = pagedir_create();
	if (t->pagedir == NULL)
		goto done;
	process_activate();

	/* Open executable file. */
	/* original code
	 file = filesys_open (file_name);
	 */
	char *buffer;
	buffer = (char *) malloc(100);
	strlcpy(buffer, file_name, strlen(file_name) + 1);

	char *token, *last;
	int argc = 0;

	token = strtok_r(buffer, " ", &last);
	file = filesys_open(token);
	while (token != NULL) {
		token = strtok_r(NULL, " ", &last);
		argc++;
	}

	if (file == NULL) {
		printf("load: %s: open failed\n", file_name);
		goto done;
	}

	file_deny_write(file);
	thread_current()->f = file;

	/* Read and verify executable header. */
	if (file_read(file, &ehdr, sizeof ehdr) != sizeof ehdr || memcmp(ehdr.e_ident, "\177ELF\1\1\1", 7) || ehdr.e_type != 2 || ehdr.e_machine != 3 || ehdr.e_version != 1 || ehdr.e_phentsize != sizeof(struct Elf32_Phdr) || ehdr.e_phnum > 1024) {
		printf("load: %s: error loading executable\n", file_name);
		goto done;
	}

	/* Read program headers. */
	file_ofs = ehdr.e_phoff;
	for (i = 0; i < ehdr.e_phnum; i++) {
		struct Elf32_Phdr phdr;

		if (file_ofs < 0 || file_ofs > file_length(file))
			goto done;
		file_seek(file, file_ofs);

		if (file_read(file, &phdr, sizeof phdr) != sizeof phdr)
			goto done;
		file_ofs += sizeof phdr;
		switch (phdr.p_type) {
		case PT_NULL:
		case PT_NOTE:
		case PT_PHDR:
		case PT_STACK:
		default:
			/* Ignore this segment. */
			break;
		case PT_DYNAMIC:
		case PT_INTERP:
		case PT_SHLIB:
			goto done;
		case PT_LOAD:
			if (validate_segment(&phdr, file)) {
				bool writable = (phdr.p_flags & PF_W) != 0;
				uint32_t file_page = phdr.p_offset & ~PGMASK;
				uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
				uint32_t page_offset = phdr.p_vaddr & PGMASK;
				uint32_t read_bytes, zero_bytes;
				if (phdr.p_filesz > 0) {
					/* Normal segment.
					 Read initial part from disk and zero the rest. */
					read_bytes = page_offset + phdr.p_filesz;
					zero_bytes = (ROUND_UP(page_offset + phdr.p_memsz, PGSIZE) - read_bytes);
				} else {
					/* Entirely zero.
					 Don't read anything from disk. */
					read_bytes = 0;
					zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE);
				}
				if (!load_segment(file, file_page, (void *) mem_page, read_bytes, zero_bytes, writable))
					goto done;
			} else
				goto done;
			break;
		}
	}

	/* Set up stack. */
	if (!setup_stack(esp))
		goto done;

//	int count = get_argc(file_name);
	bool arg_stack = push_argument(&file_name, argc, esp);

	/* Start address. */
	*eip = (void (*)(void)) ehdr.e_entry;

	success = true;

	done:
	/* We arrive here whether the load is successful or not. */
	free(buffer);
	if(!arg_stack)
		success = false;
	return success;
}


/* load() helpers. */

static bool install_page(void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
 FILE and returns true if so, false otherwise. */
static bool validate_segment(const struct Elf32_Phdr *phdr, struct file *file) {
	/* p_offset and p_vaddr must have the same page offset. */
	if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
		return false;

	/* p_offset must point within FILE. */
	if (phdr->p_offset > (Elf32_Off) file_length(file))
		return false;

	/* p_memsz must be at least as big as p_filesz. */
	if (phdr->p_memsz < phdr->p_filesz)
		return false;

	/* The segment must not be empty. */
	if (phdr->p_memsz == 0)
		return false;

	/* The virtual memory region must both start and end within the
	 user address space range. */
	if (!is_user_vaddr((void *) phdr->p_vaddr))
		return false;
	if (!is_user_vaddr((void *) (phdr->p_vaddr + phdr->p_memsz)))
		return false;

	/* The region cannot "wrap around" across the kernel virtual
	 address space. */
	if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
		return false;

	/* Disallow mapping page 0.
	 Not only is it a bad idea to map page 0, but if we allowed
	 it then user code that passed a null pointer to system calls
	 could quite likely panic the kernel by way of null pointer
	 assertions in memcpy(), etc. */
	if (phdr->p_vaddr < PGSIZE)
		return false;

	/* It's okay. */
	return true;
}

/* Loads a segment starting at offset OFS in FILE at address
 UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 memory are initialized, as follows:

 - READ_BYTES bytes at UPAGE must be read from FILE
 starting at offset OFS.

 - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

 The pages initialized by this function must be writable by the
 user process if WRITABLE is true, read-only otherwise.

 Return true if successful, false if a memory allocation error
 or disk read error occurs. */
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
	ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT(pg_ofs(upage) == 0);
	ASSERT(ofs % PGSIZE == 0);

	file_seek(file, ofs);
	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		 We will read PAGE_READ_BYTES bytes from FILE
		 and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* Get a page of memory. */
		uint8_t *kpage = palloc_get_page(PAL_USER);
		if (kpage == NULL)
			return false;

		/* Load this page. */
		if (file_read(file, kpage, page_read_bytes) != (int) page_read_bytes) {
			palloc_free_page(kpage);
			return false;
		}
		memset(kpage + page_read_bytes, 0, page_zero_bytes);

		/* Add the page to the process's address space. */
		if (!install_page(upage, kpage, writable)) {
			palloc_free_page(kpage);
			return false;
		}

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		upage += PGSIZE;
	}
	return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
 user virtual memory. */
static bool setup_stack(void **esp) {
	uint8_t *kpage;
	bool success = false;

	kpage = palloc_get_page(PAL_USER | PAL_ZERO);
	if (kpage != NULL) {
		success = install_page(((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
		if (success)
			*esp = PHYS_BASE;
		else
			palloc_free_page(kpage);
	}
	return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 virtual address KPAGE to the page table.
 If WRITABLE is true, the user process may modify the page;
 otherwise, it is read-only.
 UPAGE must not already be mapped.
 KPAGE should probably be a page obtained from the user pool
 with palloc_get_page().
 Returns true on success, false if UPAGE is already mapped or
 if memory allocation fails. */
static bool install_page(void *upage, void *kpage, bool writable) {
	struct thread *t = thread_current();

	/* Verify that there's not already a page at that virtual
	 address, then map our page there. */
	return (pagedir_get_page(t->pagedir, upage) == NULL && pagedir_set_page(t->pagedir, upage, kpage, writable));
}

struct process_file*
get_process_file_from_fd(struct thread* t, int fd) {
	struct list *list_pf = &t->list_pf;
	struct list_elem *e;
	for (e = list_begin(list_pf); e != list_end(list_pf); e = list_next(e)) {
		struct process_file
		*pf = list_entry(e, struct process_file, elem);
		if (pf->fd == fd)
			return pf;
	}
	return NULL;
}

int add_process_file(struct thread* t, struct file* file, const char* filename) {
	struct list *list_pf = &t->list_pf;
	struct process_file *pf = malloc(sizeof(struct process_file));
	if (pf == NULL)
		return -1;
	memset(pf, 0, sizeof(struct process_file));
	pf->fd = t->fd_cnt++;
	pf->file = file;
	list_push_back(list_pf, &pf->elem);
	return pf->fd;
}

void remove_process_file_from_fd(struct thread* t, int fd) {
	struct process_file* pf = get_process_file_from_fd(t, fd);
	if (pf == NULL)
		return;
	list_remove(&pf->elem);
	if (pf->file != NULL)
		file_close(pf->file);
	pf->file = NULL;
	free(pf);
}

/*
void*
malloc_print(enum struct_num num_struct)
{
	static int cnt_malloc = 0;

	void* ptr;
	switch (num_struct) {
	case AS:
		ptr = malloc(sizeof(struct arg_success));
		if (ptr == NULL)
			return NULL;
		printf("malloc as %d\n", ((struct arg_success*) ptr)->num = cnt_malloc);
		break;
	case PS:
		ptr = malloc(sizeof(struct process_status));
		if (ptr == NULL)
			return NULL;
		printf("malloc ps %d\n", ((struct process_status*) ptr)->num = cnt_malloc);
		break;
	case PF:
		ptr = malloc(sizeof(struct process_file));
		if (ptr == NULL)
			return NULL;
		printf("malloc pf %d\n", ((struct process_file*) ptr)->num = cnt_malloc);
		break;
	}
	++cnt_malloc;
	++cnt_mal_free;
	printf("CNT=%d\n", cnt_mal_free);
	return ptr;
}

void
free_print(void* ptr, enum struct_num num_struct)
{
	ASSERT(ptr != NULL);
	switch (num_struct) {
	case AS:
//		struct arg_success* as;
		printf("free as %d\n", ((struct arg_success*) ptr)->num);
		break;
	case PS:
		printf("free ps %d\n", ((struct process_status*) ptr)->num);
		break;
	case PF:
		printf("free pf %d\n", ((struct process_file*) ptr)->num);
		break;
	}
	free(ptr);
	--cnt_mal_free;
	printf("CNT=%d\n", cnt_mal_free);
}
*/

int get_argc(const char* file_name) {
	char *fn_copy = (char *) malloc(sizeof(char) * (strlen(file_name) + 1));
	strlcpy(fn_copy, file_name, strlen(file_name) + 1);

	char *token, *last;
	int argc = 0;
	for (token = strtok_r(fn_copy, " ", &last); token; token = strtok_r(NULL, " ", &last)) {
		argc++;
	}
	free(fn_copy);
	return argc;
}

bool push_argument(char ** file, int argc, void ** esp) {

	char *fn_copy = palloc_get_page(PAL_USER);
	if (fn_copy == NULL)
		return false;

	strlcpy(fn_copy, *file, PGSIZE);

	char *token, *last;
	int i = 0;
	int *index = (int *) malloc((sizeof (int)) *argc);

	for (token = strtok_r(fn_copy, " ", &last); token != NULL; token =
			strtok_r(NULL, " ", &last)) {
		index[i++] = token - fn_copy;
	}

	int size = strlen((char*) (*file));

	*esp -= size + 1;
	int pos = (int) *esp;

	for (i = 0; i <= size; i++) {
		*((char*) (*esp)) = fn_copy[i];
		*esp += 1;
	}
	*esp = pos;

	while (((int) (*esp)) % 4)
		*esp -= 1;
	*esp -= 4;
	*((int*) (*esp)) = 0;


	for (i = argc - 1; i >= 0; i--) {
		*esp -= 4;
		*((void**) (*esp)) = pos + index[i];
	}


	*esp -= 4;
	*((char **) (*esp)) = (*esp + 4);

	*esp -= 4;
	*((int *) (*esp)) = argc;

	*esp -= 4;
	*((int*) (*esp)) = 0;

	palloc_free_page(fn_copy);
	free(index);
	return true;
}

