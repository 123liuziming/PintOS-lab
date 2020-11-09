#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_halt(void) {
	shutdown_power_off();
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int * p = f->esp;

  int system_call = * p;

	switch (system_call)
	{
		case SYS_WRITE:
			if(*(p+5)==1)
				putbuf(*(p+6),*(p+7));
			break;
		case SYS_EXIT:
			thread_exit();
			break;

		case SYS_HALT:
			syscall_halt();
			break;

		default:
			printf("No match\n");
	}
}
