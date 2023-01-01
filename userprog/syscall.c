#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

#include "filesys/filesys.h"
#include "filesys/file.h"
#include <list.h>
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/synch.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

/* syscall functions */
void halt (void);
void exit (int status);
bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
tid_t fork (const char *thread_name, struct intr_frame *f);
int exec (char *file_name);

/* syscall helper functions */
void check_address(void *addr);
static struct file *find_file_by_fd(int fd);
int add_file_to_fdt(struct file *file);
void remove_file_from_fdt(int fd);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void syscall_init(void)
{
    write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
                            ((uint64_t)SEL_KCSEG) << 32);
    write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

    /* The interrupt service rountine should not serve any interrupts
     * until the syscall_entry swaps the userland stack to the kernel
     * mode stack. Therefore, we masked the FLAG_FL. */
    write_msr(MSR_SYSCALL_MASK,
              FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

    lock_init(&filesys_lock);
}

/* rax에 syscall number 가 들어있으니 이를 syscall-nr.h에 선언된 enum으로 비교 & 확인
    들어가는 인자는 단순히 들어오는 순서대로 rdi, rsi, rdx ... */
void syscall_handler(struct intr_frame *f UNUSED)
{
    switch (f->R.rax)
    {
    case SYS_HALT:
        halt();
        break;
    case SYS_EXIT:
        exit(f->R.rdi);
        break;
    case SYS_FORK:
        f->R.rax = fork(f->R.rdi, f); // 수정
        break;
    case SYS_EXEC:
        if (exec(f->R.rdi) == -1) // 수정
            exit(-1);
        break;
    case SYS_WAIT:
        f->R.rax = wait(f->R.rdi);
        break;
    case SYS_CREATE:
        f->R.rax = create(f->R.rdi, f->R.rsi);
        break;
    case SYS_REMOVE:
        f->R.rax = remove(f->R.rdi);
        break;
    case SYS_OPEN:
        f->R.rax = open(f->R.rdi);
        break;
    case SYS_FILESIZE:
        f->R.rax = filesize(f->R.rdi);
        break;
    case SYS_READ:
        f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
        break;
    case SYS_WRITE:
        f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
        break;
    case SYS_SEEK:
        seek(f->R.rdi, f->R.rsi);
        break;
    case SYS_TELL:
        f->R.rax = tell(f->R.rdi);
        break;
    case SYS_CLOSE:
        close(f->R.rdi);
        break;
    default:
        exit(-1);
        break;
    }
}

void check_address(void *addr)
{
    struct thread *current_thread = thread_current();
    /* 우선 유저영역인지 커널영역인지 확인한 뒤,
         유저영역일지라도 페이지로 할당 된 부분인지 확인 */
    if (addr = NULL||is_kernel_vaddr(addr) || pml4_get_page(current_thread->pml4, addr) == NULL)
    {
        exit(-1);
    }
}

/* fd를 통해 file을 찾는 함수 */
static struct file *find_file_by_fd(int fd)
{
    struct thread *cur = thread_current();

    /* fdtable에서 유효한 fd를 가르키지 않았다면 null 리턴 */
    if (fd < 0 || fd >= FDT_COUNT_LIMIT)
    {
        return NULL;
    }
    return cur->fd_table[fd];
}


int add_file_to_fdt(struct file *file)
{
    struct thread *cur = thread_current();
    struct file **fdt = cur->fd_table;

    /* fdt limit보다 작은지 확인 */
    while (cur->fd_idx < FDT_COUNT_LIMIT && fdt[cur->fd_idx])
    {
        cur->fd_idx++;
    }

    // error - fd table full
    if (cur->fd_idx >= FDT_COUNT_LIMIT)
        return -1;

    fdt[cur->fd_idx] = file;
    return cur->fd_idx;
}

/* fd table에서 인자로 받은 fd행을 NULL로 지우기 */
void remove_file_from_fdt(int fd)
{
    struct thread *cur = thread_current();

    if (fd < 0 || fd >= FDT_COUNT_LIMIT)
    {
        return;
    }
    
    cur->fd_table[fd] = NULL;
}


void halt(void)
{
    power_off();
}

void exit(int status)
{
    struct thread *cur = thread_current();
    cur->exit_status = status;

    printf("%s: exit(%d)\n", cur->name, status);

    thread_exit();
}

/* 자식 프로세스 종료 대기 */
int wait(tid_t tid)
{
    return process_wait(tid);
}

bool create(const char *file, unsigned initial_size)
{
    check_address(file);
    return filesys_create(file, initial_size);
}

bool remove(const char *file)
{
    check_address(file);
    return filesys_remove(file);
}

/* 파일 열기 */
int open(const char *file)
{
    check_address(file);
    lock_acquire(&filesys_lock); //수정
    struct file *open_file = filesys_open(file);

    if (open_file == NULL)
    {
        lock_release(&filesys_lock);
        return -1;
    }

    int fd = add_file_to_fdt(open_file);

    if (fd == -1)
    {
        file_close(open_file);
    }
    lock_release(&filesys_lock);
    return fd;
}

/* 자식 프로세스 생성하고 프로그램 실행 */
int exec(char *file_name)
{
    check_address(file_name);              // 파일이 유효한 주소인지 확인
    int file_size = strlen(file_name) + 1; // \0 을 위해 1더함

    /* race condition 방지하기 위해 아에 새로 할당받아 파일이름 복사해준다.
        여기서 할당한 페이지는 load에서 할당 해제 */
    char *fn_copy = palloc_get_page(PAL_ZERO);
    if (fn_copy == NULL)
    {
        exit(-1);
    }
    strlcpy(fn_copy, file_name, file_size); // file 이름만 복사

    if (process_exec(fn_copy) == -1)
    {
        return -1;
    }

    NOT_REACHED();
    return 0;
}

/* 열린 파일의 데이터를 기록 */
int write(int fd, const void *buffer, unsigned size)
{
    check_address(buffer);

    int write_result;            // return 용 wirte 한 size
    lock_acquire(&filesys_lock); //
    struct thread *curr = thread_current();//수정 rox-child
    if (fd == 0)                 // 수정
    {
        write_result = -1;
    }
    else if (fd == 1)
    {
        // if(curr->stdout_count == 0)
		// {
		// 	//Not reachable
		// 	NOT_REACHED();
		// 	remove_file_from_fdt(fd);
		// 	write_result = -1;
		// }
        // else{
        putbuf(buffer, size); // 문자열을 화면에 출력하는 함수
        write_result = size;
        //}
    }
    else
    {
        if (find_file_by_fd(fd) != NULL)
        {
            write_result = file_write(find_file_by_fd(fd), buffer, size);
        }
        else
        {
            write_result = -1;
        }
    }
    lock_release(&filesys_lock);

    return write_result;
}

/* 열린 파일 데이터 읽기 */
int read(int fd, void *buffer, unsigned size)
{
    check_address(buffer);
    off_t read_byte;
    uint8_t *read_buffer = buffer;
    struct thread *cur = thread_current();
    /* stdin 으로 들어오고있는 파일디스크립터 취급해서 읽고, stdout은 버리고 파일로 들어오는 건 직접 꺼내읽기 */

    if (fd == 0)
    {
        char key;
        for (read_byte = 0; read_byte < size; read_byte++)
        {
            key = input_getc(); // 키가 버퍼에 있으면 그걸 바로 받아오고, 없으면 들어올때까지 대기
            *read_buffer++ = key;
            if (key == '\0')
            {
                break;
            }
        }
    }
    else if (fd == 1)
    {
        return -1;
    }
    else
    {
        struct file *read_file = find_file_by_fd(fd); //
        if (read_file == NULL)
        {
            return -1;
        }
        lock_acquire(&filesys_lock);
        read_byte = file_read(read_file, buffer, size);
        lock_release(&filesys_lock);
    }
    return read_byte;
}

/* 열린 파일을 닫기 */
void close(int fd)
{
    struct file *fileobj = find_file_by_fd(fd);
    if (fileobj == NULL)
    {
        return;
    }
    remove_file_from_fdt(fd);
}

int filesize(int fd)
{
    struct file *open_file = find_file_by_fd(fd);
    if (open_file == NULL)
    {
        return -1;
    }
    return file_length(open_file);
}

/* 열린 파일의 위치(offset)를 이동 */
void seek(int fd, unsigned position)
{
    if (fd < 2)
    {
        return;
    }
    struct file *seek_file = find_file_by_fd(fd);
    //check_address(seek_file); //수정 rox-child
    file_seek(seek_file, position);
}

/* 열린 파일의 위치(offset)를 알려주기 */
unsigned tell(int fd)
{
    if (fd < 2)
    {
        return;
    }
    struct file *file = find_file_by_fd(fd);
    check_address(file);
    if (file == NULL)
    {
        return;
    }
    return file_tell(fd);
}

tid_t fork(const char *thread_name, struct intr_frame *f)
{
    return process_fork(thread_name, f);
}


/* 파일 크기 알려주기 */
/* 이 시스템콜이 어디서 사용되는지 csapp에서 찾아보기
    read 에서 메모리에 파일 올려 읽을 때 사용되기도 함 */

// write(int fd, const void *buffer, unsigned size)
// {
// 	check_address(buffer);
// 	int ret;

// 	struct file *fileobj = find_file_by_fd(fd);
// 	if (fileobj == NULL)
// 		return -1;

// 	struct thread *curr = thread_current();
	
// 	if (fileobj == 2)
// 	{
// 		if(curr->stdout_count == 0)
// 		{
// 			//Not reachable
// 			NOT_REACHED();
// 			remove_file_from_fdt(fd);
// 			ret = -1;
// 		}
// 		else
// 		{
// 			/* 버퍼를 콘솔에 출력 */
// 			putbuf(buffer, size);
// 			ret = size;
// 		}
// 	}
// 	else if (fileobj == 1)
// 	{
// 		ret = -1;
// 	}
// 	else
// 	{
// 		lock_acquire(&filesys_lock);
// 		ret = file_write(fileobj, buffer, size);
// 		lock_release(&filesys_lock);
// 	}

// 	return ret;
// }


