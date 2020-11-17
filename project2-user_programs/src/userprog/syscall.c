#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);

// syn-write
struct lock filesys_lock;

/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user (const uint8_t *uaddr)
{
  if (!is_user_vaddr(uaddr))
	return -1;
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
       : "=&a" (result) : "m" (*uaddr));
  return result;
}
 
/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
       : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

static void syscall_exit(int exit_code) {
	if (lock_held_by_current_thread(&filesys_lock))
		lock_release(&filesys_lock);
	thread_current() -> exit_code = exit_code;
	thread_exit();
}

static void check_memory(const uint8_t* addr) {
	if (get_user(addr) == -1)
		syscall_exit(EXIT_CODE_ERROR);
}

static void read_from_user(void* from, void* to, size_t size) {
	int i = 0;
	for (i = 0; i < size; ++i) {
		int tmp = get_user(from + i);
		if (tmp == -1)
			syscall_exit(EXIT_CODE_ERROR);
		*((char*) (to + i)) = tmp & 0xff;
	}
}

void
syscall_init (void) 
{
  lock_init(&filesys_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_halt(void) {
	shutdown_power_off();
}

static void syscall_write(int fd, const void* buffer, unsigned size) {
	lock_acquire(&filesys_lock);
	if (fd == 1) 
		putbuf(buffer, size);
	lock_release(&filesys_lock);
}

static int syscall_create(const char* file, unsigned initial_size) {
	lock_acquire(&filesys_lock);
	int res = filesys_create(file, initial_size);
	lock_release(&filesys_lock);
	return res;
}



static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int * p = f->esp;

  check_memory(p);

  int system_call = * p;

	switch (system_call)
	{
		case SYS_WRITE: {
			// hex_dump(f->esp, f->esp, PHYS_BASE - (f->esp), true);
			int fd;
			void* buffer;
			unsigned size;
			read_from_user(p + 1, &fd, sizeof(fd));
			read_from_user(p + 2, &buffer, sizeof(buffer));
			read_from_user(p + 3, &size, sizeof(size));
			check_memory(buffer);
			check_memory(buffer + size - 1);
			syscall_write(fd, buffer, size);
			f->eax = size;
			break;
		}
		case SYS_EXIT: {
			int exit_code;
			read_from_user(p + 1, &exit_code, sizeof(exit_code));
			syscall_exit(exit_code);
			break;
		}
		case SYS_HALT: {
			syscall_halt();
			break;
		}
		case SYS_CREATE: {
			char* file;
			unsigned initial_size;
			read_from_user(p + 1, &file, sizeof(file));
			read_from_user(p + 2, &initial_size, sizeof(initial_size));
			check_memory(file);
			f->eax = syscall_create(file, initial_size);
			break;
		}

		default:
			printf("not implement yet\n");
	}
}
