#include "userprog/umem.h"

int get_user(const uint8_t *srcva) {
  int result;
  asm("movl $1f, %0;"
      "movzbl %1, %0;"
      "1:"
      : "=&a"(result)
      : "m"(*srcva));
  return result;
}

bool put_user(uint8_t *dstva, uint8_t byte) {
  int error_code;
  asm("movl $1f, %0;"
      "movb %b2, %1;"
      "1:"
      : "=&a"(error_code), "=m"(*dstva)
      : "q"(byte));
  return error_code != -1;
}
