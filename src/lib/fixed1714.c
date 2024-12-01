#include <debug.h>
#include <fixed1714.h>

fixed1714_t fixed1714(int numerator, int denominator) {
  ASSERT(denominator != 0);
  fixed1714_t f;
  f._raw = (numerator << FRACTION_BITS) / denominator;
  return f;
}

int fixed1714_to_int_zero(fixed1714_t x) { return x._raw >> FRACTION_BITS; }

int fixed1714_to_int_round(fixed1714_t x) {
  int f_2 = ((1 << FRACTION_BITS) / 2);
  if (x.sign) {
    return (x._raw - f_2) >> FRACTION_BITS;
  } else {
    return (x._raw + f_2) >> FRACTION_BITS;
  }
}

fixed1714_t fixed1714_add(fixed1714_t x, fixed1714_t y) {
  fixed1714_t z;
  z._raw = x._raw + y._raw;
  return z;
}

fixed1714_t fixed1714_add_int(fixed1714_t x, int n) {
  fixed1714_t z;
  z._raw = x._raw + (n << FRACTION_BITS);
  return z;
}

fixed1714_t fixed1714_sub(fixed1714_t x, fixed1714_t y) {
  fixed1714_t z;
  z._raw = x._raw - y._raw;
  return z;
}

fixed1714_t fixed1714_sub_int(fixed1714_t x, int n) {
  fixed1714_t z;
  z._raw = x._raw - (n << FRACTION_BITS);
  return z;
}

fixed1714_t fixed1714_mul(fixed1714_t x, fixed1714_t y) {
  fixed1714_t z;
  z._raw = (((int64_t)x._raw) * y._raw) >> FRACTION_BITS;
  return z;
}

fixed1714_t fixed1714_mul_int(fixed1714_t x, int n) {
  fixed1714_t z;
  z._raw = x._raw * n;
  return z;
}

fixed1714_t fixed1714_div(fixed1714_t x, fixed1714_t y) {
  fixed1714_t z;
  z._raw = (((int64_t)x._raw) << FRACTION_BITS) / y._raw;
  return z;
}

fixed1714_t fixed1714_div_int(fixed1714_t x, int n) {
  fixed1714_t z;
  z._raw = x._raw / n;
  return z;
}
