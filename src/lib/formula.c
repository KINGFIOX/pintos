#include <formula.h>

#include "fixed1714.h"
#include "threads/thread.h"

int formula_priority(fixed1714_t recent_cpu, int nice) {
  fixed1714_t rc_4 = fixed1714_div_int(recent_cpu, 4);
  fixed1714_t nice_2 = fixed1714_mul_int(fixed1714(nice, 1), 2);
  fixed1714_t pri_max = fixed1714(PRI_MAX, 1);
  fixed1714_t pri = fixed1714_sub(pri_max, rc_4);
  pri = fixed1714_sub(pri, nice_2);
  int pri_int = fixed1714_to_int_round(pri);
  return pri_int;
}

fixed1714_t formula_load_avg(fixed1714_t load_avg, int ready_threads) {
  fixed1714_t la_59_60 = fixed1714_mul(fixed1714(59, 60), load_avg);
  fixed1714_t rt_60 = fixed1714_div_int(fixed1714(ready_threads, 1), 60);
  return fixed1714_add(la_59_60, rt_60);
}

fixed1714_t formula_recent_cpu(fixed1714_t recent_cpu, fixed1714_t load_avg, int nice) {
  fixed1714_t two_la = fixed1714_mul_int(load_avg, 2);
  fixed1714_t coefficient = fixed1714_div(two_la, fixed1714_add_int(two_la, 1));
  fixed1714_t recent_cpu_coe = fixed1714_mul(coefficient, recent_cpu);
  return fixed1714_add_int(recent_cpu_coe, nice);
}