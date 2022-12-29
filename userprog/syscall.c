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
#include "userprog/process.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include "filesys/file.c"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void check_address(void *addr);


bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
void halt(void);
void exit(int status);
struct lock filesys_lock;
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

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);
    
	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
    lock_init(&filesys_lock);
}
void
check_address (void *addr) {
	struct thread *current_thread = thread_current();
	
	if (is_kernel_vaddr(addr) || pml4_get_page(current_thread->pml4, addr) == NULL) {
		exit(-1);
	}
}

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
		// fork();
		break;
    case SYS_EXEC:
		exec(f->R.rdi);
        break;
    case SYS_WAIT:
        process_wait(f->R.rdi);
        break;
    case SYS_CREATE:
        create(f->R.rdi,f->R.rsi);
        break;
    case SYS_REMOVE:
        remove(f->R.rdi);
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
        //f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
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

void halt(void){
	power_off();
}

void exit(int status)
{
	struct thread *cur = thread_current();
	cur->exit_status = status;
	
	printf("%s : exit(%d)",cur->name,status);
	thread_exit();
}

bool create(const char *file, unsigned initial_size)
{
	check_address(file);
	return filesys_create(file,initial_size);
}

bool remove(const char *file)
{
	check_address(file);
	return filesys_remove(file);
}

int exec(char *file_name)
{
    check_address(file_name); //유효한 주소인지 확인
    int file_size = strlen(file_name) + 1; //NULL 센티넬을 위해 1을 더함
    char *fn_copy = palloc_get_page(PAL_ZERO); //
    if (fn_copy == NULL)
    {
        exit(-1);
    }
    strlcpy(fn_copy, file_name, file_size); // file 이름만 복사

    if (process_exec(fn_copy) == -1) //process_exec이 에러나면 -1
    {
        return -1;
    }

    NOT_REACHED();
    return 0;
}

int wait(tid_t pid)
{
    return process_wait(pid);
};

//fd를 이용해서 file을 찾는 함수
static struct file *find_file_by_fd(int fd)
{
    struct thread *cur = thread_current();
    if (fd < 0 || fd >= FDT_COUNT_LIMIT) //fd가 유효하면
    {
        return NULL;
    }
    return cur->fd_table[fd]; //해당 fd의 파일을 리턴
}

//현재 프로세스의 fd table에 file을 추가하는 함수
int add_file_to_fdt(struct file *file)
{
    struct thread *cur = thread_current();
    struct file **fdt = cur -> fd_table;

    while(cur->fd_idx < FDT_COUNT_LIMIT && fdt[cur->fd_idx])
    {
        cur -> fd_idx;
    }
    if(cur->fd_idx >= FDT_COUNT_LIMIT)
        return -1;
    fdt[cur->fd_idx] = file; //파일의 주소가 fdt의 해당 인덱스에 저장됨
    return cur->fd_idx;
}

//fd table에서 파일을 제거하는 함수
void remove_file_from_fdt(int fd)
{
    struct thread *cur = thread_current();

    if(fd<0 || fd >= FDT_COUNT_LIMIT)
        return;

    cur->fd_table[fd] = NULL;
}

int open(const char *file)
{
    check_address(file);
    struct file *open_file = filesys_open(file);

    if (open_file == NULL)
    {
        return -1;
    }
    //fdt에 연 파일 넣어주기
    int fd = add_file_to_fdt(open_file);

    if(fd == -1) //fdt가 가득 찼을 때
    {
        file_close(open_file); //파일을 닫아버림
    }
    return fd;
}

int filesize(int fd) //왜 사용???
{
    struct file *open_file = find_file_by_fd(fd);
    if(open_file == NULL)
    {
        return -1;
    }
    return file_length(open_file);
}

int read(int fd, void *buffer, unsigned size)
{
    check_address(buffer); //유효한 주소인지 확인
    off_t read_byte; 
    uint8_t *read_buffer = buffer;
    if(fd ==0) //표준 입력
    {
        char key;
        for (read_byte = 0; read_byte < size; read_byte++)
        {
            input_getc(); //버퍼에 키가 있으면 그걸 받아오고, 없으면 들어올때까지 대기
            *read_buffer++ = key;
            if(key == '\0')
            {
                break;
            }
        }
    }
    else if(fd == 1) //표준 출력
    {
        return -1;
    }
    else{
        struct file *read_file = find_file_by_fd(fd);
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

//파일의 pos필드를 position으로 설정해주는 함수
void seek(int fd, unsigned position)
{
    if(fd <= 2)
    {
        return;
    }
    struct file *seek_file = find_file_by_fd(fd);
    check_address(seek_file);
    file_seek(seek_file, position);
}

//파일의 현재 위치를 알려주는 함수
unsigned tell(int fd)
{
    if(fd <= 2)
    {
        return;
    }
    struct file *tell_file = find_file_by_fd(fd);
    check_address(tell_file);
    if(tell_file == NULL)
    {
        return;
    }
    return file_tell(fd);
}

//fd로 file을 찾아서 fdt에서 지워버리는 함수
void close(int fd)
{   
    struct file *fileobj = find_file_by_fd(fd);
    if(fileobj == NULL)
    {
        return;
    }
    remove_file_from_fdt(fd);
}

tid_t fork(const char *thread_name, struct intr_frame *f)
{
    return process_fork(thread_name, f);
}
