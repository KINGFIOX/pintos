#include "userprog/syscall.h"

#include <stddef.h>
#include <stdio.h>
#include <syscall-nr.h>

#include "threads/interrupt.h"
#include "threads/thread.h"

typedef long (*syscall_t)(const uint32_t args[]);

static syscall_t syscall_table[] = {
    [SYS_WRITE] = NULL,
};

static void syscall_handler(struct intr_frame *f) {
  uint32_t *esp = f->esp;
  long syscall_no = esp[0];
  uint32_t *args = &esp[1];
  if (SYS_HALT <= syscall_no && syscall_no <= SYS_INUMBER) {
    syscall_t routine = syscall_table[syscall_no];
    if (routine != NULL) {
      f->eax = routine(args);
    } else {
      PANIC("system call not implemented!");
    }
  } else {
    printf("not a valid system call!\n");
    thread_exit();
  }
}

void syscall_init(void) { intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall"); }