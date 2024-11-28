#include <debug.h>
#include <heap.h>
#include <stddef.h>

// 交换两个 entry_t 的值
static void swap(struct heap_elem *arr[], int x, int y) {
  struct heap_elem *temp = arr[x];
  arr[x] = arr[y];
  arr[y] = temp;
}

static void heapify_up(struct heap *heap, int idx) {
  int parent = (idx - 1) / 2;
  if (idx && heap->predicate(heap->_arr[idx]->key, heap->_arr[parent]->key)) {
    swap(heap->_arr, parent, idx);
    heapify_up(heap, parent);
  }
}

static void heapify_down(struct heap *heap, int idx) {
  int left = 2 * idx + 1;
  int right = 2 * idx + 2;
  int smallest = idx;
  if (left < heap->length && heap->predicate(heap->_arr[left]->key, heap->_arr[smallest]->key)) smallest = left;
  if (right < heap->length && heap->predicate(heap->_arr[right]->key, heap->_arr[smallest]->key)) smallest = right;
  if (smallest != idx) {
    swap(heap->_arr, smallest, idx);
    heapify_down(heap, smallest);
  }
}

void heap_push_back(struct heap *heap, struct heap_elem *elem) {
  if (heap->length == heap->capacity) PANIC("heap full\n");
  heap->_arr[heap->length] = elem;
  heap->length++;
  heapify_up(heap, heap->length - 1);
}

struct heap_elem *heap_pop_front(struct heap *heap) {
  if (heap->length <= 0) return NULL;
  if (heap->length == 1) {
    heap->length = 0;
    return heap->_arr[0];
  }
  struct heap_elem *root = heap->_arr[0];
  heap->_arr[0] = heap->_arr[heap->length - 1];
  heap->length--;
  heapify_down(heap, 0);
  heap->_arr[heap->length] = NULL;
  return root;
}

struct heap_elem *heap_front(struct heap *heap) {
  if (heap->length <= 0) return NULL;
  return heap->_arr[0];
}

static bool _min(int a, int b) { return a < b; }
static bool _max(int a, int b) { return a > b; }

void heap_init(struct heap *heap, int capacity, bool is_min) {
  heap->length = 0;
  heap->capacity = capacity;
  heap->predicate = is_min ? _min : _max;
}