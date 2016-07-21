#ifndef _SEC_TO_CYCLES_H
#define _SEC_TO_CYCLES_H

#include <stdio.h>
#include <sys/time.h>

#include <rte_cycles.h>

void
cycles_to_sec_init(void);

static uint64_t
time_diff_in_us(uint64_t new_tsc, uint64_t old_tsc);

static double
time_diff_in_s(uint64_t new_tsc, uint64_t old_tsc);

#endif
