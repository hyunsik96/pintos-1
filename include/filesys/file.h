#ifndef FILESYS_FILE_H
#define FILESYS_FILE_H

#include "filesys/off_t.h"
#include <stdbool.h>
struct inode;
/* An open file. */
struct file {
	struct inode *inode;        /* File's inode. 파일 데이터의 메타 데이터*/
	off_t pos;                  /* Current position. 파일 읽거나 쓸 때 처음부터 읽거나 쓸 필요 없다*/
	bool deny_write;            /* Has file_deny_write() been called? 읽기전용 파일인지 나태냄*/
};
/* Opening and closing files. */
struct file *file_open (struct inode *);
struct file *file_reopen (struct file *);
struct file *file_duplicate (struct file *file);
void file_close (struct file *);
struct inode *file_get_inode (struct file *);

/* Reading and writing. */
off_t file_read (struct file *, void *, off_t);
off_t file_read_at (struct file *, void *, off_t size, off_t start);
off_t file_write (struct file *, const void *, off_t);
off_t file_write_at (struct file *, const void *, off_t size, off_t start);

/* Preventing writes. */
void file_deny_write (struct file *);
void file_allow_write (struct file *);

/* File position. */
void file_seek (struct file *, off_t);
off_t file_tell (struct file *);
off_t file_length (struct file *);

#endif /* filesys/file.h */
