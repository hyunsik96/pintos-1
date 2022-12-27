#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void check_address(void *addr);


bool create(const char *file, unsigned initial_size);
bool remove(const char *file);
void halt(void);
void exit(int status);

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
		// exec();
        break;
    case SYS_WAIT:
        // process_wait();
        break;
    case SYS_CREATE:
        create(f->R.rdi,f->R.rsi);
        break;
    case SYS_REMOVE:
        remove(f->R.rdi);
        break;
    case SYS_OPEN:
        // open();
        break;
    case SYS_FILESIZE:
        // filesize();
        break;
    case SYS_READ:
        // read();
        break;
    case SYS_WRITE:
        // write();
        break;
    case SYS_SEEK:
        // seek();
        break;
    case SYS_TELL:
        // tell();
        break;
    case SYS_CLOSE:
        // close();
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
	cur->status = status;
	
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