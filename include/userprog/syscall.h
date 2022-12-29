#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdbool.h>
#include <threads/thread.h>

void syscall_init (void);

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

void check_address(void *addr);

void halt(void);
void exit(int status);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);

int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

int exec (char *file_name);
int wait (tid_t pid);
tid_t fork (const char *thread_name, struct intr_frame *f);

static struct file *find_file_by_fd(int fd);
int add_file_to_fdt(struct file *file);
void remove_file_from_fdt(int fd);

struct lock filesys_lock;

#endif /* userprog/syscall.h */
