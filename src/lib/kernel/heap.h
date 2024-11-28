#ifndef __LIB_KERNEL_HEAP_H
#define __LIB_KERNEL_HEAP_H

#include <stdbool.h>

struct heap_elem {
  int key;
};

struct heap {
  int length;
  int capacity;
  bool (*predicate)(int, int);
  struct heap_elem *_arr[0];  // flexible array member
};

void heap_push_back(struct heap *heap, struct heap_elem *elem);
struct heap_elem *heap_pop_front(struct heap *heap);
struct heap_elem *heap_front(struct heap *heap);
void heap_init(struct heap *heap, int capacity, bool is_min);

#endif
