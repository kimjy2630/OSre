#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

////
static int get_user(const uint8_t *uaddr);
static bool put_user(uint8_t *udst, uint8_t byte);
static bool read_validity(const void *uaddr, int size);
static bool write_validity(const void* udst, int size);
static void* get_argument(void *ptr, int pos);

static void halt(void);
static void exit(int status);
static pid_t exec(const char *file);
static int wait(pid_t pid);
static bool create(const char *file, unsigned initial_size);
static bool remove(const char *file);
static int open(const char *file);
static int filesize(int fd);
static int read(int fd, void *buffer, unsigned length);
static int write(int fd, const void *buffer, unsigned length);
static void seek(int fd, unsigned position);
static unsigned tell(int fd);
static void close(int fd);
////

#endif /* userprog/syscall.h */
