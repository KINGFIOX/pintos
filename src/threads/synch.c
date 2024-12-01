/** This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/** Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"

#include <stdio.h>
#include <string.h>

#include "kernel/list.h"
#include "stddef.h"
#include "threads/interrupt.h"
#include "threads/thread.h"

/* ---------- ---------- semaphore ---------- ----------  */

/** Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void sema_init(struct semaphore *sema, unsigned value) {
  ASSERT(sema != NULL);

  sema->value = value;
  list_init(&sema->waiters);  // 初始化等待队列
}

/** Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void sema_down(struct semaphore *sema) {
  ASSERT(sema != NULL);
  ASSERT(!intr_context());

  enum intr_level old_level = intr_disable();
  while (sema->value == 0) {
    list_push_back(&sema->waiters, &thread_current()->elem);
    thread_block();
  }
  sema->value--;
  intr_set_level(old_level);
}

/** Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool sema_try_down(struct semaphore *sema) {
  enum intr_level old_level;
  bool success;

  ASSERT(sema != NULL);

  old_level = intr_disable();
  if (sema->value > 0) {
    sema->value--;
    success = true;
  } else
    success = false;
  intr_set_level(old_level);

  return success;
}

static bool sema_less_func(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
  struct thread *t1 = container_of(a, struct thread, elem);
  struct thread *t2 = container_of(b, struct thread, elem);
  return t1->priority < t2->priority;
}

static struct thread *sema_pop_max_priority_thread(struct list *list) {
  struct list_elem *elem = list_max(list, sema_less_func, NULL);
  ASSERT(elem != NULL);
  list_remove(elem);
  elem->next = NULL;
  elem->prev = NULL;
  return container_of(elem, struct thread, elem);
}

static void __sema_up(struct semaphore *sema) {
  enum intr_level old_level;

  ASSERT(sema != NULL);

  old_level = intr_disable();
  if (!list_empty(&sema->waiters)) {
    if (!thread_mlfqs()) {  ////////////////////////////////////////////////////
      struct thread *t = sema_pop_max_priority_thread(&sema->waiters);
      ASSERT(t != NULL);
      thread_unblock(t);
    } else {  //////////////////////////////////////////////////////////////////

      // TODO: mlfq
      thread_unblock(container_of(list_pop_front(&sema->waiters), struct thread, elem));
    }
  }
  sema->value++;
  intr_set_level(old_level);
}

void sema_up_intr(struct semaphore *sema) {
  ASSERT(intr_context());
  __sema_up(sema);
}

/** Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void sema_up(struct semaphore *sema) {
  if (!thread_mlfqs()) {  ////////////////////////////////////////////////////
    ASSERT(!intr_context());
    __sema_up(sema);
    thread_yield();
  } else {  /////////////////////////////////////////////////////////////////

    // TODO: mlfqs
    __sema_up(sema);
  }
}

static void sema_test_helper(void *sema_);

/** Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void sema_self_test(void) {
  struct semaphore sema[2];
  int i;

  printf("Testing semaphores...");
  sema_init(&sema[0], 0);
  sema_init(&sema[1], 0);
  thread_create("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) {
    sema_up(&sema[0]);
    sema_down(&sema[1]);
  }
  printf("done.\n");
}

/** Thread function used by sema_self_test(). */
static void sema_test_helper(void *sema_) {
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) {
    sema_down(&sema[0]);
    sema_up(&sema[1]);
  }
}

/* ---------- ---------- lock ---------- ----------  */

/** Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void lock_init(struct lock *lock) {
  ASSERT(lock != NULL);

  lock->holder = NULL;
  sema_init(&lock->semaphore, 1);
}

struct buffer_elem {
  struct list_elem elem;
  struct thread *thread;
};

static struct list *elem_in_list(struct list_elem *e) {
  while (e->prev != NULL) {
    e = e->prev;
  }
  return container_of(e, struct list, head);
}

static bool contains_holder(struct list *buffer, struct thread *holder) {
  for (struct list_elem *e = list_begin(buffer); e != list_end(buffer); e = list_next(e)) {
    if (e == &holder->elem) {
      return true;
    }
  }
  return false;
}

static void __update_priority_r(struct thread *holder, int priority, struct list *buffer) {
  if (contains_holder(buffer, holder)) {
    return;  // already in buffer list
  }
  if (holder->priority < priority) {  // updated
    holder->priority = priority;
    holder->donated = true;
    if (holder->status == THREAD_BLOCKED) {                                       // recursively update
      struct list *waiters = elem_in_list(&holder->elem);                         // get the header of waiters list
      struct semaphore *sema = container_of(waiters, struct semaphore, waiters);  // get the semaphore
      struct lock *lock = container_of(sema, struct lock, semaphore);             // get the lock
      struct thread *next_holder = lock->holder;                                  // get the next holder

      struct buffer_elem be;  // push next holder to buffer list
      be.thread = next_holder;
      list_push_back(buffer, &be.elem);
      __update_priority_r(next_holder, priority, buffer);
      list_pop_back(buffer);
    }
  }
}

static void priority_donate(struct thread *holder, int priority) {
  struct list buffer;  // init buffer list
  list_init(&buffer);
  struct buffer_elem be;  // push holder to buffer list
  be.thread = holder;
  list_push_back(&buffer, &be.elem);
  __update_priority_r(holder, priority, &buffer);
  list_pop_back(&buffer);
}

static bool contains_lock(struct thread *cur, struct lock *lck) {
  for (struct list_elem *e = list_begin(&cur->locks); e != list_end(&cur->locks); e = list_next(e)) {
    if (e == &lck->elem) {
      return true;
    }
  }
  return false;
}

static void thread_push_lock(struct thread *cur, struct lock *lck) {
  ASSERT(cur != NULL);
  ASSERT(lck != NULL);
  ASSERT(!contains_lock(cur, lck));
  list_push_back(&cur->locks, &lck->elem);
}

static void thread_pop_lock(struct thread *cur, struct lock *lck) {
  ASSERT(cur != NULL);
  ASSERT(lck != NULL);
  ASSERT(contains_lock(cur, lck));
  list_remove(&lck->elem);
  lck->elem.next = NULL;
  lck->elem.prev = NULL;
}

/** Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void lock_acquire(struct lock *lock) {
  ASSERT(lock != NULL);
  ASSERT(!intr_context());
  ASSERT(!lock_held_by_current_thread(lock));

  if (!thread_mlfqs()) {  ////////////////////////////////////////////////////
    struct thread *cur = thread_current();
    struct thread *holder = lock->holder;

    if (holder != NULL) {                      // 🔐 被占用
      if (holder->priority < cur->priority) {  // 优先级反转
        priority_donate(holder, cur->priority);
        // holder->priority = cur->priority;
      }
    }

    sema_down(&lock->semaphore);  // ---------- 进入临界区 ----------
    lock->holder = thread_current();
    thread_push_lock(cur, lock);
  } else {  ////////////////////////////////////////////////////////////////

    // TODO: mlfqs
    sema_down(&lock->semaphore);
    lock->holder = thread_current();
  }
}

/** Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool lock_try_acquire(struct lock *lock) {
  bool success;

  ASSERT(lock != NULL);
  ASSERT(!lock_held_by_current_thread(lock));

  if (!thread_mlfqs()) {  ////////////////////////////////////////////////////
    struct thread *cur = thread_current();
    success = sema_try_down(&lock->semaphore);  // ----------  ----------
    if (success) {
      lock->holder = thread_current();
      list_push_back(&cur->locks, &lock->elem);
    }
  } else {  ////////////////////////////////////////////////////////////////

    // TODO: mlfqs
    success = sema_try_down(&lock->semaphore);
    if (success) lock->holder = thread_current();
  }
  return success;
}

/** return -1 means: no waiters */
static int max_priority_in_waiters(struct list *waiters) {
  int max = -1;
  for (struct list_elem *e = list_begin(waiters); e != list_end(waiters); e = list_next(e)) {
    struct thread *t = container_of(e, struct thread, elem);
    if (t->priority > max) {
      max = t->priority;
    }
  }
  return max;
}

static int next_priority(struct lock *lock) {
  int max = -1;
  for (struct list_elem *e = list_begin(&lock->holder->locks); e != list_end(&lock->holder->locks); e = list_next(e)) {
    struct lock *lk = container_of(e, struct lock, elem);
    struct list *waiters = &lk->semaphore.waiters;
    int max_in_waiters = max_priority_in_waiters(waiters);
    if (max_in_waiters > max) {
      max = max_in_waiters;
    }
  }
  return max;
}

/** Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void lock_release(struct lock *lock) {
  ASSERT(lock != NULL);
  ASSERT(lock_held_by_current_thread(lock));

  if (!thread_mlfqs()) {  ////////////////////////////////////////////////////
    thread_pop_lock(lock->holder, lock);
    int next_pri = next_priority(lock);
    if (next_pri == -1) {
      next_pri = lock->holder->before_donated_priority;
      lock->holder->donated = false;
    }
    lock->holder->priority = next_pri;
    lock->holder = NULL;        // clear holder
    sema_up(&lock->semaphore);  // ---------- 退出临界区 ----------
  } else {                      /////////////////////////////////////////////

    // TODO: mlfqs
    lock->holder = NULL;
    sema_up(&lock->semaphore);
  }
}

/** Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool lock_held_by_current_thread(const struct lock *lock) {
  ASSERT(lock != NULL);

  return lock->holder == thread_current();
}

/* ---------- ---------- condition variable ---------- ----------  */

/** One semaphore in a list. */
struct semaphore_elem {
  struct list_elem elem;      /**< List element. */
  struct semaphore semaphore; /**< This semaphore. */
};

/** Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void cond_init(struct condition *cond) {
  ASSERT(cond != NULL);

  list_init(&cond->waiters);
}

/** Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void cond_wait(struct condition *cond, struct lock *lock) {
  struct semaphore_elem waiter;

  ASSERT(cond != NULL);
  ASSERT(lock != NULL);
  ASSERT(!intr_context());
  ASSERT(lock_held_by_current_thread(lock));

  sema_init(&waiter.semaphore, 0);
  list_push_back(&cond->waiters, &waiter.elem);  // NOTE: cond_wait list_push_back; cond_signal list_pop_front
  lock_release(lock);
  sema_down(&waiter.semaphore);
  lock_acquire(lock);
}

void dump_sema_waiters(const struct list *waiters, int indent) {
  printf("%*ssema waiters (struct thread): {\n", indent, "");
  for (struct list_elem *e = list_begin((struct list *)waiters); e != list_end((struct list *)waiters); e = list_next(e)) {
    struct thread *t = container_of(e, struct thread, elem);
    printf(" %*s[%d] = ", indent, "", t->tid);
    dump_thread(t, indent + 1);
  }
  printf("%*s},\n", indent, "");
}

void dump_cond_waiters(const struct list *waiters, int indent) {
  printf("%*scond waiters (struct semaphore_elem): {\n", indent, "");
  for (struct list_elem *e = list_begin((struct list *)waiters); e != list_end((struct list *)waiters); e = list_next(e)) {
    struct semaphore_elem *sema_elem = container_of(e, struct semaphore_elem, elem);
    printf(" %*s[%d] = ", indent, "", sema_elem->semaphore.value);
    dump_sema_waiters(&sema_elem->semaphore.waiters, indent + 1);
  }
  printf("%*s},\n", indent, "");
}

static bool condvar_less_func(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED) {
  struct semaphore_elem *sema_a = container_of(a, struct semaphore_elem, elem);  // 根据 dump 的信息, 一个 sema 的 waiters 只有一个元素
  struct semaphore_elem *sema_b = container_of(b, struct semaphore_elem, elem);
  ASSERT(list_size(&sema_a->semaphore.waiters) == 1);
  ASSERT(list_size(&sema_b->semaphore.waiters) == 1);
  struct thread *t_a = container_of(list_front(&sema_a->semaphore.waiters), struct thread, elem);
  ASSERT(t_a != NULL);
  struct thread *t_b = container_of(list_front(&sema_b->semaphore.waiters), struct thread, elem);
  ASSERT(t_b != NULL);
  return t_a->priority < t_b->priority;
}

static struct semaphore_elem *condvar_pop_max_priority_thread(struct list *list) {
  struct list_elem *elem = list_max(list, condvar_less_func, NULL);
  ASSERT(elem != NULL);
  list_remove(elem);
  elem->next = NULL;
  elem->prev = NULL;
  return container_of(elem, struct semaphore_elem, elem);
}

/** If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void cond_signal(struct condition *cond, struct lock *lock UNUSED) {
  ASSERT(cond != NULL);
  ASSERT(lock != NULL);
  ASSERT(!intr_context());
  ASSERT(lock_held_by_current_thread(lock));

  if (!list_empty(&cond->waiters)) {
    if (!thread_mlfqs()) {  //////////////////////////////////////////////////
      // printf("---------- cond_signal ----------\n");
      // dump_cond_waiters(&cond->waiters, 0);
      struct semaphore_elem *sema_elem = condvar_pop_max_priority_thread(&cond->waiters);
      sema_up(&sema_elem->semaphore);
    } else {  ////////////////////////////////////////////////////////////////

      // TODO: mlfqs
      sema_up(&container_of(list_pop_front(&cond->waiters), struct semaphore_elem, elem)->semaphore);
    }
  }
}

/** Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void cond_broadcast(struct condition *cond, struct lock *lock) {
  ASSERT(cond != NULL);
  ASSERT(lock != NULL);

  while (!list_empty(&cond->waiters)) {
    cond_signal(cond, lock);
  }
}
