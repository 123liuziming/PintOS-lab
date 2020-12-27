#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

struct lock filesys_lock;
void syscall_init (void);

static void syscall_halt(void);
void syscall_munmap(int mapping);

#endif /* userprog/syscall.h */
