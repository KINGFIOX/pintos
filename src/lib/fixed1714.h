#ifndef __LIB_FIXED1714_H
#define __LIB_FIXED1714_H

#define FRACTION_BITS 14
#define INTEGER_BITS (32 - FRACTION_BITS - 1)

#include <stdint.h>

typedef union {
  int32_t _raw;
  struct {
    int32_t fraction : FRACTION_BITS;
    int32_t integer : INTEGER_BITS;
    int32_t sign : 1;
  };
} fixed1714_t;

fixed1714_t fixed1714(int numerator, int denominator);
int fixed1714_to_int_zero(fixed1714_t x);
int fixed1714_to_int_round(fixed1714_t x);
fixed1714_t fixed1714_add(fixed1714_t x, fixed1714_t y);
fixed1714_t fixed1714_add_int(fixed1714_t x, int n);
fixed1714_t fixed1714_sub(fixed1714_t x, fixed1714_t y);
fixed1714_t fixed1714_sub_int(fixed1714_t x, int n);
fixed1714_t fixed1714_mul(fixed1714_t x, fixed1714_t y);
fixed1714_t fixed1714_mul_int(fixed1714_t x, int n);
fixed1714_t fixed1714_div(fixed1714_t x, fixed1714_t y);
fixed1714_t fixed1714_div_int(fixed1714_t x, int n);

#endif
