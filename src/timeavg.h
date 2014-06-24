#include "redis.h"

#ifndef __TIMEAVG_H
#define __TIMEAVG_H

#define TA_BUCKETS 20

struct time_average {
	uint32_t last_updated;
	uint32_t buckets[TA_BUCKETS];
};

#endif