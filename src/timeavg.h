#include "redis.h"

#ifndef __TIMEAVG_H
#define __TIMEAVG_H

//Number of buckets in time average
#define TA_BUCKETS 20

//Number of buckets in unique time average
#define TU_BUCKETS 6

typedef struct {
	uint32_t last_updated;
	uint32_t buckets[TA_BUCKETS];
} time_average;

typedef struct {
	uint32_t last_updated;
	uint32_t created_time;
	robj* buckets[TU_BUCKETS];
} unique_time_average;

#endif