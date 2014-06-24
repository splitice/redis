#include "redis.h"

#ifndef __TIMEAVG_H
#define __TIMEAVG_H

#define TA_BUCKETS 20

typedef struct {
	uint32_t last_updated;
	uint32_t buckets[TA_BUCKETS];
} time_average;

#endif