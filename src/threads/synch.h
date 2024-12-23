#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

#include "kernel/list.h"

/** A counting semaphore. */
struct semaphore {
  unsigned value;      /**< Current value. */
  struct list waiters; /**< List of waiting threads. */
};

void sema_init(struct semaphore *sema, unsigned value);
void sema_down(struct semaphore *sema);
bool sema_try_down(struct semaphore *sema);
void sema_up(struct semaphore *sema);
void sema_up_intr(struct semaphore *sema);
void sema_self_test(void);

/** Lock. */
struct lock {
  struct thread *holder;      /**< Thread holding lock (for debugging). */
  struct semaphore semaphore; /**< Binary semaphore controlling access. */
  struct list_elem elem;      /**< the elem in thread->locks */
};

void lock_init(struct lock *);
void lock_acquire(struct lock *);
bool lock_try_acquire(struct lock *);
void lock_release(struct lock *);
bool lock_held_by_current_thread(const struct lock *);

/** Condition variable. */
struct condition {
  struct list waiters; /**< List of waiting threads. */
};

void cond_init(struct condition *);
void cond_wait(struct condition *, struct lock *);
void cond_signal(struct condition *, struct lock *);
void cond_broadcast(struct condition *, struct lock *);

void dump_sema_waiters(const struct list *waiters, int indent);
void dump_cond_waiters(const struct list *waiters, int indent);

/** Optimization barrier.

   The compiler will not reorder operations across an
   optimization barrier.  See "Optimization Barriers" in the
   reference guide for more information.*/
#define barrier() asm volatile("" : : : "memory")

#endif /**< threads/synch.h */
