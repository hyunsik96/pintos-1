#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include "threads/thread.h"

void syscall_init (void);
void check_address (void *addr);
void syscall_handler(struct intr_frame *f UNUSED);

void halt(void);
void exit(int status);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);

int exec(char *file_name);
int wait(tid_t pid);

static struct file *find_file_by_fd(int fd);
int add_file_to_fdt(struct file *file);
void remove_file_from_fdt(int fd);

int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
tid_t fork(const char *thread_name, struct intr_frame *f);

#endif /* userprog/syscall.h */
