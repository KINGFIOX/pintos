#include "threads/switch.h"

#### struct thread *switch_threads (struct thread *cur, struct thread *next);
####
#### Switches from CUR, which must be the running thread, to NEXT,
#### which must also be running switch_threads(), returning CUR in
#### NEXT's context.
####
#### This function works by assuming that the thread we're switching
#### into is also running switch_threads().  Thus, all it has to do is
#### preserve a few registers on the stack, then switch stacks and
#### restore the registers.  As part of switching stacks we record the
#### current stack pointer in CUR's thread structure.

.globl switch_entry
.globl switch_threads

.globl thread_stack_ofs # defined in thread/thread.c
.globl thread_schedule_tail # defined in thread/thread.c

.func switch_threads
switch_threads:
	# Save caller's register state.
	#
	# Note that the SVR4 ABI allows us to destroy %eax, %ecx, %edx,
	# but requires us to preserve %ebx, %ebp, %esi, %edi.  See
	# [SysV-ABI-386] pages 3-11 and 3-12 for details.
	#
	# This stack frame must match the one set up by thread_create() in size.
	# save callee-saved registers
	pushl %ebx
	pushl %ebp
	pushl %esi
	pushl %edi

	# Get offsetof (struct thread, stack).
	mov thread_stack_ofs, %edx

	# Save current stack pointer to old thread's stack, if any.
	# SWITCH_CUR = 20, a macro defined in threads/switch.h
	# next             24(%esp)
	# cur              20(%esp)
	# <return address> 16(%esp)
	# %ebx             12(%esp)
	# %ebp             8(%esp)
	# %esi             4(%esp)
	# %edi             0(%esp)
	movl SWITCH_CUR(%esp), %eax # cur. as return value
	movl %esp, (%eax,%edx,1) # cur->stack = %esp

	# Restore stack pointer from new thread's stack.
	# SWITCH_NEXT = 24, a macro defined in threads/switch.h
	movl SWITCH_NEXT(%esp), %ecx # next
	movl (%ecx,%edx,1), %esp # %esp = next->stack

	# Restore caller's register state.
	popl %edi
	popl %esi
	popl %ebp
	popl %ebx
    ret
.endfunc

.func switch_entry
switch_entry:
	# Discard switch_threads() arguments.
	addl $8, %esp

	# Call thread_schedule_tail(prev).
	pushl %eax
	call thread_schedule_tail
	addl $4, %esp

	# Start thread proper.
	ret
.endfunc
