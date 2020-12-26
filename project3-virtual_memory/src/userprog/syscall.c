#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "filesys/filesys.h"
#include "threads/vaddr.h"
#include "devices/input.h"
#include "filesys/file.h"
#include "userprog/process.h"
#include "threads/init.h"
#include "vm/frame.h"

static void syscall_handler (struct intr_frame *);

struct file 
  {
    struct inode *inode;        /* File's inode. */
    off_t pos;                  /* Current position. */
    bool deny_write;            /* Has file_deny_write() been called? */
  };

// syn-write
struct lock filesys_lock;


#ifdef VM
// 把页面pin住,防止在读写系统调用的时候发生中断
void pre_pin_pages(void* buffer, int size) {
	void* st;
	struct thread* t = thread_current();
	void* pagedir = t->pagedir;
	struct vm_sup_page_table * table = t->spt;
	for (st = pg_round_down(buffer); st < buffer + size; st += PGSIZE) {
		vm_get_page(pagedir, st, table);
		struct vm_sup_page_table_entry* entry = find_supt_entry(table, st);
		if (!entry)
			continue;
		vm_frame_pin(entry->p_addr);
	}
}

void pre_unpin_pages(void* buffer, int size) {
	void* st;
	struct thread* t = thread_current();
	void* pagedir = t->pagedir;
	struct vm_sup_page_table * table = t->spt;
	for (st = pg_round_down(buffer); st < buffer + size; st += PGSIZE) {
		struct vm_sup_page_table_entry* entry = find_supt_entry(table, st);
		if (!entry)
			continue;
		vm_frame_pin(entry->p_addr);;
	}
}
#endif


static bool check_fd(int fd) {
	return fd >= 0 && fd < 128;
}

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
	int i;
	struct thread* cur = thread_current();
	pid_hash_map[cur->tid]->exit_code = exit_code;
	cur->exit_code = exit_code;
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

static int syscall_write(int fd, const void* buffer, unsigned size) {
	int res = -1;
	lock_acquire(&filesys_lock);
	if (fd == 1) { 
		putbuf(buffer, size);
		res = size;
	}
	else {
		if (check_fd(fd)) {
			struct file* f = thread_current() -> files_map[fd];
			if (f) {
			#ifdef VM
				pre_pin_pages(buffer, size);
			#endif
				res = file_write(f, buffer, size);
			#ifdef VM
				pre_unpin_pages(buffer, size);
			#endif
			}
		}
	}
	lock_release(&filesys_lock);
	return res;
}

static int syscall_create(const char* file, unsigned initial_size) {
	lock_acquire(&filesys_lock);
	int res = filesys_create(file, initial_size);
	lock_release(&filesys_lock);
	return res;
}

static int syscall_read(int fd, void* buffer, unsigned size) {
	int res = -1;
	lock_acquire(&filesys_lock);
	if (fd == 0) {
		int i = 0;
		for (i = 0; i < size; ++i) {
			if (!put_user(buffer + i, input_getc())) {
				lock_release(&filesys_lock);
				syscall_exit(EXIT_CODE_ERROR);
			}
		}
		res = size;
	} else {
		if (check_fd(fd)) {
			struct file* f = thread_current() -> files_map[fd];
			if (f) {
			#ifdef VM
				pre_pin_pages(buffer, size);
			#endif
				res = file_read(f, buffer, size);
			#ifdef VM
				pre_unpin_pages(buffer, size);
			#endif
			}
		}
	}
	lock_release(&filesys_lock);
	return res;
}

static int syscall_open(const char* file) {
	if (strcmp(file, thread_current()->exec_file_name) == 0)
		return 127;
	lock_acquire(&filesys_lock);
	struct file* f = filesys_open(file);
	if (!f) {
		lock_release(&filesys_lock);
		return -1;
	}
	thread_current() -> files_map[fd_now++] = f;
	lock_release(&filesys_lock);
	return fd_now - 1;
}

static int syscall_filesize(int fd) {
	int res = -1;
	lock_acquire(&filesys_lock);
	if (check_fd(fd)) {
		struct file* f = thread_current() -> files_map[fd];
		if (f) 
			res = file_length(f);
	}
	lock_release(&filesys_lock);
	return res;
}

static void syscall_close(int fd) {
	lock_acquire(&filesys_lock);
	if (check_fd(fd)) {
		struct file* f = thread_current()->files_map[fd];
		if (f && fd != 127) {
			file_close(f);
			thread_current()->files_map[fd] = NULL;
		}
	}
	lock_release(&filesys_lock);
}

static pid_t syscall_exec(void* cmd) {
	pid_t res = process_execute(cmd); 
	return res;
}

static int syscall_wait(pid_t pid) {
	int res = process_wait(pid);
	return res;
}

static void syscall_seek(int fd, unsigned position) {
	lock_acquire(&filesys_lock);
	if (check_fd(fd)) {
		struct file* f = thread_current() -> files_map[fd];
		if (f) {
			file_seek(f, position);
		}
	}
	lock_release(&filesys_lock);
}

static unsigned syscall_tell(int fd) {
	unsigned res = -1;
	lock_acquire(&filesys_lock);
	if (check_fd(fd)) {
		struct file* f = thread_current() -> files_map[fd];
		if (f) {
			res = file_tell(f);
		}
	}
	lock_release(&filesys_lock);
	return res;
}

static bool syscall_remove(const char *file) {
	bool res;
	lock_acquire (&filesys_lock);
  	res = filesys_remove(file);
  	lock_release (&filesys_lock);
  	return res;
}

#ifdef VM
int syscall_mmap(int fd, void* addr) {
	if (!addr || pg_ofs(addr) != 0) {
		goto MMAP_FAIL;
	}
	if (!check_fd(fd)) {
		goto MMAP_FAIL;
	}
	struct thread* t = thread_current();
	struct file* f = t->files_map[fd];
	if (!f) {
		goto MMAP_FAIL;
	}
	lock_acquire(&filesys_lock);
	struct file* f_reopen = file_reopen(f);
	int len = file_length(f_reopen);
	void* st = addr;
	struct vm_sup_page_table *table = t->spt;
	int offset = 0;
	for (; st < st + len; st += PGSIZE) {
		if (find_supt_entry(t->supt, st)) {
			goto MMAP_FAIL;
		}
		int read_bytes = len - offset >= PGSIZE ? PGSIZE : len - offset;
		int zero_bytes = PGSIZE - read_bytes;
		vm_alloc_page_from_filesys(table, st, f_reopen, offset, read_bytes, zero_bytes, true);
		offset += PGSIZE;
	}
	struct mmap_desc* save = (struct mmap_desc*) malloc(sizeof(struct mmap_desc));
	save->file = f_reopen;
	save->u_addr = addr;
	save->mmap_id = t->mmap_cnt;
	t->mmap_list[(t->mmap_cnt)++] = save;
	lock_release(&filesys_lock);
	return t->mmap_cnt - 1;
MMAP_FAIL:
	lock_release(&filesys_lock);
	return -1;
}

void syscall_munmap(int mapping) {
	// 要解绑的对象
	struct thread* t = thread_current();
	struct mmap_desc* target = t->mmap_list[mapping];
	int len = file_length(target->file);
	int offset = 0;
	void* st = entry->addr;

MUMNAP_FAIL:
	lock_release(&filesys_lock);
}

#endif

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int * p = f->esp;

  check_memory(p);
  
  //hex_dump(f->esp, f->esp,PHYS_BASE - (f->esp),true);
  int system_call = * p;
  // 保存原始栈
  thread_current()->origin_stack = f->esp;

	switch (system_call)
	{
		case SYS_WRITE: {
			int fd;
			void* buffer;
			unsigned size;
			read_from_user(p + 1, &fd, sizeof(fd));
			read_from_user(p + 2, &buffer, sizeof(buffer));
			read_from_user(p + 3, &size, sizeof(size));
			check_memory(buffer);
			check_memory(buffer + size - 1);
			f->eax = syscall_write(fd, buffer, size);
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
		case SYS_READ: {
			int fd;
			void* buffer;
			unsigned size;
			read_from_user(p + 1, &fd, sizeof(fd));
			read_from_user(p + 2, &buffer, sizeof(buffer));
			read_from_user(p + 3, &size, sizeof(size));
			check_memory(buffer);
			check_memory(buffer + size - 1);
			f->eax = syscall_read(fd, buffer, size);
			break;
		}
		case SYS_OPEN: {
			char* file;
			read_from_user(p + 1, &file, sizeof(file));
			check_memory(file);
			f->eax = syscall_open(file);
			break;
		}
		case SYS_FILESIZE: {
			int fd;
			read_from_user(p + 1, &fd, sizeof(fd));
			f->eax = syscall_filesize(fd);
			break;
		}
		case SYS_CLOSE: {
			int fd;
			read_from_user(p + 1, &fd, sizeof(fd));
			syscall_close(fd);
			break;
		}
		case SYS_EXEC: {
			void* cmd;
			read_from_user(p + 1, &cmd, sizeof(cmd));
			check_memory(cmd);
			f->eax = syscall_exec(cmd);
			break;
		}
		case SYS_WAIT: {
			pid_t pid;
			read_from_user(p + 1, &pid, sizeof(pid));
			f->eax = syscall_wait(pid);
			break;
		}
		case SYS_SEEK: {
			int fd;
			unsigned position;
			read_from_user(p + 1, &fd, sizeof(fd));
			read_from_user(p + 2, &position, sizeof(position));
			syscall_seek(fd, position);
			break;
		}
		case SYS_TELL: {
			int fd;
			read_from_user(p + 1, &fd, sizeof(fd));
			f->eax = syscall_tell(fd);
			break;
		}
		case SYS_REMOVE: {
			char* file;
			read_from_user(p + 1, &file, sizeof(file));
			check_memory(file);
			f->eax = syscall_remove(file);
			break;
		}

		default:
			printf("%d\n", system_call);
			printf("not implement yet\n");
	}
}
