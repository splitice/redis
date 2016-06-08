#include "redis.h"

#ifndef __TIMEAVG_H
#define __TIMEAVG_H

//Number of buckets in time average
#define TA_BUCKETS 32

//Number of buckets in unique time average
#define TU_BUCKETS 8

typedef struct {
	uint32_t last : 24;
	uint8_t  interval     : 8;
} ta_time;

typedef struct {
	uint32_t buckets[TA_BUCKETS];
	ta_time time;
} time_average;

typedef struct {
	uint32_t last_updated;
	uint32_t created_time;
	robj*    buckets[TU_BUCKETS];
} unique_time_average;

#endif