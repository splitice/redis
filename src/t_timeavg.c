/*
Time Average Datastructure for Redis.
=====================================

This data structure provides the ability to efficiently produce an average rate of events, and distinct events occuring
over a set time frame. This value can then be divided over time to produce an average rate.

- "ta*" commands track the occurance of events over time
- "tu*" commands track unique events happening over time

# Event Commands:
## tahit [interval] [by] [timestamp] [key1] [key2...]
Signal that the event has happened, increment each [key] by the the value of [by] within the scope of [timestamp].

Returns the new sum over time value for each key (bulk reply) based on the current [timestamp]. The [interval] 
between buckets (how much time does a bucket represent) must be provided for this calculation.

## tacalc [interval] [timestamp] [key]
Returns the sum over time value for a given key  based on the current [timestamp]. The [interval] between buckets
(how much time does a bucket represent) must be provided for this calculation.

# Unique Event Commands:
## tuhit [interval] [unique value] [timestamp] [key1] [key2...]
Signal that the event has happened, with the [unique value]. Increment each [key] if the [unique value] is unique within
the scope of [timestamp].

Returns the new sum over time value for each key (bulk reply) based on the current [timestamp]. The [interval]
between buckets (how much time does a bucket represent) must be provided for this calculation.

## tuupdate [interval] [unique values] [timestamp] [key1] [key2...]
Signal that the event has happened, with the each value in [unique values]. Increment each [key] if the unique value is 
unique within the scope of [timestamp]. A unique value is extracted from [unique values]. [unique values] is split by the
0x00 (\x00) character. This function is not binary safe for [unique values].

Returns OK if successful.

## tucalc [interval] [timestamp] [key]
Returns the sum of unqiue events over time value for a given key  based on the current [timestamp]. The [interval] 
between buckets (how much time does a bucket represent) must be provided for this calculation.

Data Structure:
	 ---------------------------------------------
	 |               |         |         |
	 |  Last Updated | Bucket0 | Bucket1 | ....
	 |     32bit     |         |         |
	 |               |         |         |
	 ---------------------------------------------

 - "TA" types utilize a 32bit unsigned integer bucket value
 - "TU" types utilize a redis hyperloglog object as a bucket value (pointer)

 The number of buckets is detimrined by a compile time constants.

 - TA_BUCKETS defaults to 20
 - TU_BUCKETS defaults to 7

 # Memory Requirements
 - Time average objects require 84 bytes of memory per object.
 - Time unique average objects require UP TO 421Kb of memory per object.
*/
#include "redis.h"
#include "hyperloglog.h"
#include <math.h>
#include "timeavg.h"

static char *invalid_hll_err = "-INVALIDOBJ Corrupted HLL object detected\r\n";

robj *createTaObject(void) {
	unsigned char *zl = zcalloc(sizeof(time_average));
	robj *o = createObject(REDIS_TAVG, zl);
	return o;
}

robj *createTuObject(void) {
	unique_time_average* zl_uta = (unique_time_average*)zcalloc(sizeof(unique_time_average));
	robj *o = createObject(REDIS_TUAVG, (unsigned char *)zl_uta);
	for (int i = 0; i < TU_BUCKETS; i++){
		zl_uta->buckets[i] = NULL;
	}
	return o;
}

robj *taTypeLookupWriteOrCreate(redisClient *c, robj *key) {
	robj *o = lookupKeyWrite(c->db, key);
	if (o == NULL) {
		o = createTaObject();
		dbAdd(c->db, key, o);
	}
	else {
		if (o->type != REDIS_TAVG) {
			addReply(c, shared.wrongtypeerr);
			return NULL;
		}
	}
	return o;
}

robj *tuTypeLookupWriteOrCreate(redisClient *c, robj *key) {
	robj *o = lookupKeyWrite(c->db, key);
	if (o == NULL) {
		o = createTuObject();
		dbAdd(c->db, key, o);
	}
	else {
		if (o->type != REDIS_TUAVG) {
			addReply(c, shared.wrongtypeerr);
			return NULL;
		}
	}
	return o;
}

void tahitCommand(redisClient *c) {
	long bucket_interval, by, ts;

	//the bucket
	if ((getLongFromObjectOrReply(c, c->argv[1], &bucket_interval, NULL) != REDIS_OK))
		return; 

	if ((getLongFromObjectOrReply(c, c->argv[2], &by, NULL) != REDIS_OK))
		return;

	if ((getLongFromObjectOrReply(c, c->argv[3], &ts, NULL) != REDIS_OK))
		return;
	
	addReplyMultiBulkLen(c, c->argc - 4);

	unsigned int bucketN = (ts / bucket_interval) % TA_BUCKETS;

	for (int i = 4; i < c->argc; i++){
		robj *o = taTypeLookupWriteOrCreate(c, c->argv[i]);


		if (o == NULL || o->type != REDIS_TAVG) {
			addReply(c, shared.nullbulk);
			continue;
		}

		time_average* ta = (time_average*)o->ptr;
		ta->buckets[bucketN] += by;

		//difference between the begining of the previously updated bucket and now.
		//int limits the max time a value can be stale
		int updated_ago = ((ts / bucket_interval) * bucket_interval) - ta->last_updated;

		if (updated_ago > bucket_interval){
			unsigned int clear_buckets = updated_ago / bucket_interval;

			if (clear_buckets >= TA_BUCKETS){
				clear_buckets = TA_BUCKETS - 1;
			}

			for (unsigned int i = 1; i < clear_buckets; i++){
				unsigned int k = (bucketN - i) % TA_BUCKETS;
				ta->buckets[k] = 0;
			}

			ta->buckets[bucketN] = 1;
		}

		ta->last_updated = (unsigned int)ts;

		signalModifiedKey(c->db, c->argv[1]);
		notifyKeyspaceEvent(REDIS_NOTIFY_LIST, "tahit", c->argv[i], c->db->id);
		server.dirty++;

		long long sum = 0;
		for (unsigned int i = 0; i < TA_BUCKETS; i++){
			sum += ta->buckets[i];
		}

		addReplyBulkLongLong(c, sum);
	}
}

void tacalcCommand(redisClient *c){

	long bucket_interval, ts;

	if ((getLongFromObjectOrReply(c, c->argv[1], &bucket_interval, NULL) != REDIS_OK))
		return;

	if ((getLongFromObjectOrReply(c, c->argv[2], &ts, NULL) != REDIS_OK))
		return;

	robj *o;
	if ((o = lookupKeyReadOrReply(c, c->argv[3], shared.nokeyerr)) == NULL || checkType(c, o, REDIS_TAVG)){
		return;
	}

	time_average* ta = (time_average*)o->ptr;

	int updated_ago = ((ts / bucket_interval) * bucket_interval) - ta->last_updated;
	unsigned int bucketN = (ts / bucket_interval) % TA_BUCKETS;
	
	long long sum = 0;
	if (updated_ago < 0){
		for (unsigned int i = 0; i < TA_BUCKETS; i++){
			sum += ta->buckets[i];
		}
	}
	else{
		unsigned int clear_buckets = updated_ago / bucket_interval;

		//If we need to clear all buckets, then the value will be 0
		if (clear_buckets < TA_BUCKETS){
			for (; clear_buckets < TA_BUCKETS; clear_buckets++){
				unsigned int k = (bucketN - clear_buckets) % TA_BUCKETS;
				sum += ta->buckets[k];
			}
		}
	}
	
	addReplyLongLong(c, sum);
}


void tuhitCommand(redisClient *c) {
	long bucket_interval, ts;

	//the bucket
	if ((getLongFromObjectOrReply(c, c->argv[1], &bucket_interval, NULL) != REDIS_OK))
		return;

	if ((getLongFromObjectOrReply(c, c->argv[3], &ts, NULL) != REDIS_OK))
		return;

	robj* unique = c->argv[2];

	addReplyMultiBulkLen(c, c->argc - 4);

	unsigned int bucketN = (ts / bucket_interval) % TU_BUCKETS;

	for (int i = 4; i < c->argc; i++){
		robj *o = tuTypeLookupWriteOrCreate(c, c->argv[i]);

		if (o == NULL || o->type != REDIS_TUAVG) {
			addReply(c, shared.nullbulk);
			continue;
		}

		unique_time_average* ta = (unique_time_average*)o->ptr;


		//difference between the begining of the previously updated bucket and now.
		//int limits the max time a value can be stale
		int updated_ago = ((ts / bucket_interval) * bucket_interval) - ta->last_updated;

		if (updated_ago > bucket_interval){
			unsigned int clear_buckets = updated_ago / bucket_interval;

			if (clear_buckets >= TU_BUCKETS){
				clear_buckets = TU_BUCKETS - 1;
			}

			for (unsigned int i = 0; i < clear_buckets; i++){
				unsigned int k = (bucketN - i) % TU_BUCKETS;
				robj* r = ta->buckets[k];
				if (r != NULL){
					sdsfree(r->ptr);
					zfree(r);
					ta->buckets[k] = NULL;
				}
			}
		}

		robj* nr = ta->buckets[bucketN];
		if (nr == NULL){
			nr = createHLLObject();
			ta->buckets[bucketN] = nr;
		}

		if (hllAdd(nr, (unsigned char*)unique->ptr, sdslen(unique->ptr))){
			int invalid = 0;
			struct hllhdr *hdr = nr->ptr;
			/* Recompute it and update the cached value. */
			uint64_t card = hllCount(hdr, &invalid);
			if (invalid) {
				addReplySds(c, sdsnew(invalid_hll_err));
				return;
			}
			hdr->card[0] = card & 0xff;
			hdr->card[1] = (card >> 8) & 0xff;
			hdr->card[2] = (card >> 16) & 0xff;
			hdr->card[3] = (card >> 24) & 0xff;
			hdr->card[4] = (card >> 32) & 0xff;
			hdr->card[5] = (card >> 40) & 0xff;
			hdr->card[6] = (card >> 48) & 0xff;
			hdr->card[7] = (card >> 56) & 0xff;
		}

		ta->last_updated = (unsigned int)ts;


		long long sum = 0;
		for (unsigned int i = 0; i < TU_BUCKETS; i++){
			robj* r = ta->buckets[i]; 
			if (r == NULL){
				continue;
			}

			struct hllhdr *hdr = r->ptr;
			uint64_t card;

			/* Just return the cached value. */
			card = (uint64_t)hdr->card[0];
			card |= (uint64_t)hdr->card[1] << 8;
			card |= (uint64_t)hdr->card[2] << 16;
			card |= (uint64_t)hdr->card[3] << 24;
			card |= (uint64_t)hdr->card[4] << 32;
			card |= (uint64_t)hdr->card[5] << 40;
			card |= (uint64_t)hdr->card[6] << 48;
			card |= (uint64_t)hdr->card[7] << 56;

			sum += card;
		}

		signalModifiedKey(c->db, c->argv[1]);
		notifyKeyspaceEvent(REDIS_NOTIFY_LIST, "tuhit", c->argv[i], c->db->id);
		server.dirty++;

		addReplyBulkLongLong(c, sum);
	}
}

void tucalcCommand(redisClient *c){
	long bucket_interval, ts;

	if ((getLongFromObjectOrReply(c, c->argv[1], &bucket_interval, NULL) != REDIS_OK))
		return;

	if ((getLongFromObjectOrReply(c, c->argv[2], &ts, NULL) != REDIS_OK))
		return;

	robj *o;
	if ((o = lookupKeyReadOrReply(c, c->argv[3], shared.nokeyerr)) == NULL || checkType(c, o, REDIS_TUAVG)){
		return;
	}

	unique_time_average* ta = (unique_time_average*)o->ptr;

	int updated_ago = ((ts / bucket_interval) * bucket_interval) - ta->last_updated;
	unsigned int bucketN = (ts / bucket_interval) % TU_BUCKETS;
	unsigned int clear_buckets = updated_ago / bucket_interval;

	if (clear_buckets >= TU_BUCKETS){
		addReplyLongLong(c, 0);
		return;
	}

	if (clear_buckets < 0){
		clear_buckets = 0;
	}

	long long sum = 0;
	for (; clear_buckets < TU_BUCKETS; clear_buckets++){
		unsigned int k = (bucketN - clear_buckets) % TA_BUCKETS;
		robj* r = ta->buckets[k];
		if (r == NULL){
			continue;
		}

		struct hllhdr *hdr = r->ptr;
		uint64_t card;

		/* Just return the cached value. */
		card = (uint64_t)hdr->card[0];
		card |= (uint64_t)hdr->card[1] << 8;
		card |= (uint64_t)hdr->card[2] << 16;
		card |= (uint64_t)hdr->card[3] << 24;
		card |= (uint64_t)hdr->card[4] << 32;
		card |= (uint64_t)hdr->card[5] << 40;
		card |= (uint64_t)hdr->card[6] << 48;
		card |= (uint64_t)hdr->card[7] << 56;

		sum += card;
	}

	addReplyLongLong(c, sum);
}

void tuupdateCommand(redisClient *c) {
	long bucket_interval, ts;

	//the bucket
	if ((getLongFromObjectOrReply(c, c->argv[1], &bucket_interval, NULL) != REDIS_OK))
		return;

	if ((getLongFromObjectOrReply(c, c->argv[3], &ts, NULL) != REDIS_OK))
		return;

	robj* unique = c->argv[2];

	unsigned int bucketN = (ts / bucket_interval) % TU_BUCKETS;

	for (int i = 4; i < c->argc; i++){
		robj *o = tuTypeLookupWriteOrCreate(c, c->argv[i]);

		if (o == NULL || checkType(c,o,REDIS_TUAVG)) {
			continue;
		}

		unique_time_average* ta = (unique_time_average*)o->ptr;


		//difference between the begining of the previously updated bucket and now.
		//int limits the max time a value can be stale
		int updated_ago = ((ts / bucket_interval) * bucket_interval) - ta->last_updated;

		if (updated_ago > bucket_interval){
			unsigned int clear_buckets = updated_ago / bucket_interval;

			if (clear_buckets >= TU_BUCKETS){
				clear_buckets = TU_BUCKETS - 1;
			}

			for (unsigned int i = 0; i < clear_buckets; i++){
				unsigned int k = (bucketN - i) % TU_BUCKETS;
				robj* r = ta->buckets[k];
				if (r != NULL){
					sdsfree(r->ptr);
					zfree(r);
					ta->buckets[k] = NULL;
				}
			}
		}

		robj* nr = ta->buckets[bucketN];
		if (nr == NULL){
			nr = createHLLObject();
			ta->buckets[bucketN] = nr;
		}


		unsigned char* uniqueptr = (unsigned char*)unique->ptr;
		size_t uniquelen = sdslen(unique->ptr);	

		int hasMore = 1, updated = 0;
		do{
			unsigned char* temp_uniqueptr = uniqueptr;
			for (size_t f = 0; f < uniquelen; f++){
				if (*temp_uniqueptr == 0x00){
					break;
				}
				temp_uniqueptr++;
			}
			size_t difference = temp_uniqueptr - uniqueptr;
			uniquelen -= difference;
			if (uniquelen == 0){
				hasMore = 0;
			}
			if (hllAdd(nr, uniqueptr, difference)){
				updated = 1;
			}
		} while (hasMore);

		if (updated){
			int invalid = 0;
			struct hllhdr *hdr = nr->ptr;
			/* Recompute it and update the cached value. */
			uint64_t card = hllCount(hdr, &invalid);
			if (invalid) {
				addReplySds(c, sdsnew(invalid_hll_err));
				return;
			}
			hdr->card[0] = card & 0xff;
			hdr->card[1] = (card >> 8) & 0xff;
			hdr->card[2] = (card >> 16) & 0xff;
			hdr->card[3] = (card >> 24) & 0xff;
			hdr->card[4] = (card >> 32) & 0xff;
			hdr->card[5] = (card >> 40) & 0xff;
			hdr->card[6] = (card >> 48) & 0xff;
			hdr->card[7] = (card >> 56) & 0xff;
		}

		ta->last_updated = (unsigned int)ts;

		addReply(c, shared.ok);

		signalModifiedKey(c->db, c->argv[1]);
		notifyKeyspaceEvent(REDIS_NOTIFY_LIST, "tuupdate", c->argv[i], c->db->id);
		server.dirty++;
	}
}