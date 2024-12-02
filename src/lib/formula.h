#ifndef __LIB_FORMULA_H
#define __LIB_FORMULA_H

#include <fixed1714.h>

// FORMULA: priority = PRI_MAX - recent_cpu / 4 - 2 * nice
int formula_priority(fixed1714_t recent_cpu, int nice);

// FORMULA: load_avg = 59/60 * load_avg + 1/60 * ready_threads
fixed1714_t formula_load_avg(fixed1714_t load_avg, int ready_threads);

// FORMULA: recent_cpu = (2 * load_avg)/(2 * load_avg + 1) * recent_cpu + nice
fixed1714_t formula_recent_cpu(fixed1714_t recent_cpu, fixed1714_t load_avg, int nice);

#endif  // __LIB_FORMULA_H
