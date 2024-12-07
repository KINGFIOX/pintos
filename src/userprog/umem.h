#ifndef USERPROG_UMEM_H
#define USERPROG_UMEM_H

#include <stdbool.h>
#include <stdint.h>

int get_user(const uint8_t *srcva);
bool put_user(uint8_t *dstva, uint8_t byte);

#endif