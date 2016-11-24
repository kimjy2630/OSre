#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "userprog/process.h"

typedef int mapid_t;

struct lock lock_file;

void syscall_init (void);


////
static int get_user(const uint8_t *uaddr);
static bool put_user(uint8_t *udst, uint8_t byte);
static bool read_validity(const void *uaddr, int size);
static bool write_validity(const void* udst, int size);
static void* get_argument(void *ptr, int pos);

void halt(void);
void exit(int status);
pid_t exec(const char *file);
int wait(pid_t pid);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned length);
int write(int fd, const void *buffer, unsigned length);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
////
mapid_t mmap(int fd, uint8_t *uaddr);
void munmap(mapid_t mapping);

#endif /* userprog/syscall.h */
