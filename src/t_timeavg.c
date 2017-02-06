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

## tahitx [interval] [by] [timestamp] [key1] [key2...]
Same as tahit, but expiring automatically

## tacalc [timestamp] [key]
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

Also returns the time this structure was reated

# Data Structure:

The number of buckets is detimrined by a compile time constants.

- TA_BUCKETS defaults to 32
- TU_BUCKETS defaults to 8

## Time Average
	 ---------------------------------------------
	 |               |         |         |
	 |  Last Updated | Bucket0 | Bucket1 | ....
	 |     32bit     |  32bit  |  32bit  |
	 |               |         |         |
	 ---------------------------------------------

## Time Unique Average
	 ------------------------------------------------------------
	 |               |              |         |         |
	 |  Last Updated | Time Created | Bucket0 | Bucket1 | ....
	 |     32bit     |     32bit    |   HLL   |   HLL   |
	 |               |              |         |         |
	 ------------------------------------------------------------


 # Memory Requirements
 - Time average objects require 68 bytes of memory per object.
 - Time unique average objects require UP TO 73kb of memory per object.
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

robj *createTuObject(uint32_t timestamp) {
	unique_time_average* zl_uta = (unique_time_average*)zcalloc(sizeof(unique_time_average));
	zl_uta->created_time = timestamp;
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

robj *tuTypeLookupWriteOrCreate(redisClient *c, robj *key, uint32_t timestamp) {
	robj *o = lookupKeyWrite(c->db, key);
	if (o == NULL) {
		o = createTuObject(timestamp);
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

void _tahitCommand(redisClient *c, int expire) {
	long bucket_interval, by, bucketDiff;
	long long ts;
	unsigned int bucketN;
	uint32_t bucketAbsolute;
	long long sum, expireTime;
	time_average* ta;
	robj *o;

	//the bucket interval (time per bucket)
	if ((getLongFromObjectOrReply(c, c->argv[1], &bucket_interval, NULL) != REDIS_OK))
		return; 

	//the ammount to increment by
	if ((getLongFromObjectOrReply(c, c->argv[2], &by, NULL) != REDIS_OK))
		return;

	//the current timestamp
	if ((getLongLongFromObjectOrReply(c, c->argv[3], &ts, NULL) != REDIS_OK))
		return;
	
	//all the remaining arguments are keys, there will be one reply per key
	addReplyMultiBulkLen(c, c->argc - 4);

	//the current bucket
	bucketAbsolute = (ts / bucket_interval) % 16777216;
	bucketN = bucketAbsolute % TA_BUCKETS;
	if (expire){
		expireTime = mstime() + (bucket_interval * TA_BUCKETS * 1000);
	}

	//starting at argument 4, iterate all arguments: these are the keys
	for (int i = 4; i < c->argc; i++){
		//make sure this is a timavg (TA) key, then cast
		o = taTypeLookupWriteOrCreate(c, c->argv[i]);
		if (o == NULL || o->type != REDIS_TAVG) {
			addReply(c, shared.nullbulk);
			continue;
		}
		
		if (expire){
			setExpire(c->db, c->argv[i], expireTime);
		}
		
		ta = (time_average*)o->ptr;

		//difference between the begining of the previously updated bucket and now.
		//int limits the max time a value can be stale
		bucketDiff = bucketAbsolute - ta->time.last;

		//If updated more than one bucket interval ago, we need to clear a bucket in between
		if (ta->time.interval != bucket_interval){
			bucketDiff = TA_BUCKETS;
			ta->time.interval = bucket_interval;
		}
		if (bucketDiff > 0){
			//Calculate number of buckets to clear
			if (bucketDiff >= TA_BUCKETS){
				bucketDiff = TA_BUCKETS;
			}

			//Clear some buckets
			unsigned int f = bucketN;
			for (unsigned int g = 0; g < bucketDiff; g++){
				ta->buckets[f] = 0;
				if (f == 0) {
					f = TA_BUCKETS;
				}
				f--;
			}

			//Set our new bucket
			ta->buckets[bucketN] = by;

			//Set the time last updated
			//todo: handle overflows
			ta->time.last = bucketAbsolute;
		}
		else if(bucketDiff > -TA_BUCKETS)
		{
			//Increment our bucket
			ta->buckets[bucketN] += by;
		}
		else {
			goto sum;
		}

		//Redis database "stuff"
		signalModifiedKey(c->db, o);
		notifyKeyspaceEvent(REDIS_NOTIFY_LIST, "tahit", c->argv[i], c->db->id);
		server.dirty++;

sum:
		//Calculate sum
		sum = 0;
		for (unsigned int g = 0; g < TA_BUCKETS; g++){
			sum += ta->buckets[g];
		}

		//Send reply for key (the sum)
		addReplyBulkLongLong(c, sum);
	}
}

//tahit [interval] [by] [timestamp] [key1] [key2...]
void tahitCommand(redisClient *c) {
	_tahitCommand(c, 0);
}

//tahitx [interval] [by] [timestamp] [key1] [key2...]
void tahitxCommand(redisClient *c) {
	_tahitCommand(c, 1);
}

//tacalc [timestamp] [key]
void tacalcCommand(redisClient *c){
	uint32_t bucketDiff;
	long long ts;
	unsigned int bucketN;
	uint32_t bucketAbsolute;
	long long sum = 0;
	time_average* ta;
	robj *o;

	//the current timestamp
	if ((getLongLongFromObjectOrReply(c, c->argv[1], &ts, NULL) != REDIS_OK))
		return;

	//the key
	if ((o = lookupKeyReadOrReply(c, c->argv[2], shared.nokeyerr)) == NULL || checkType(c, o, REDIS_TAVG)){
		return;
	}
	ta = (time_average*)o->ptr;

	//calculations
	bucketAbsolute = ts / ta->time.interval;
	bucketN = bucketAbsolute % TA_BUCKETS;
	bucketDiff = (uint32_t)((bucketAbsolute % 16777216) - ta->time.last;

	//We only need to do reversed "clearing" if bucketDiff is greater than one bucket
	if (bucketDiff > TA_BUCKETS){
		//If we need to clear all buckets, then the value will be 0
		goto return_val;
	}
	else if(bucketDiff < 0) {
		bucketDiff = 0;//Negative guard
	}

	bucketDiff = TA_BUCKETS - bucketDiff;

	//Sum up from bucketN to num_buckets (wrapped)
	unsigned int f = bucketN;
	for (unsigned int i = 0; i<bucketDiff; i++){
		sum += ta->buckets[f];
		if (++f == TA_BUCKETS) {
			f = 0;
		}
	}

return_val:
	//reply with sum
	addReplyLongLong(c, sum);
}


void tuhitCommand(redisClient *c) {
	long bucket_interval;
	long long ts;

	//the bucket
	if ((getLongFromObjectOrReply(c, c->argv[1], &bucket_interval, NULL) != REDIS_OK))
		return;

	if ((getLongLongFromObjectOrReply(c, c->argv[3], &ts, NULL) != REDIS_OK))
		return;

	uint32_t timestamp = (uint32_t)ts;

	robj* unique = c->argv[2];

	addReplyMultiBulkLen(c, c->argc - 4);

	unsigned int bucketN = (ts / bucket_interval) % TU_BUCKETS;

	for (int i = 4; i < c->argc; i++){
		robj *o = tuTypeLookupWriteOrCreate(c, c->argv[i], timestamp);

		if (o == NULL || o->type != REDIS_TUAVG) {
			addReply(c, shared.nullbulk);
			continue;
		}

		unique_time_average* ta = (unique_time_average*)o->ptr;


		//difference between the begining of the previously updated bucket and now.
		//int limits the max time a value can be stale
		int bucketDiff = ((ts / bucket_interval) * bucket_interval) - ta->last_updated;

		if (bucketDiff > bucket_interval){
			unsigned int clear_buckets = bucketDiff / bucket_interval;

			if (clear_buckets >= TU_BUCKETS){
				clear_buckets = TU_BUCKETS;
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

		ta->last_updated = timestamp;


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

		signalModifiedKey(c->db, c->argv[i]);
		notifyKeyspaceEvent(REDIS_NOTIFY_LIST, "tuhit", c->argv[i], c->db->id);
		server.dirty++;

		addReplyBulkLongLong(c, sum);
	}
}

void tucalcCommand(redisClient *c){
	long bucket_interval;
	long long ts;

	if ((getLongFromObjectOrReply(c, c->argv[1], &bucket_interval, NULL) != REDIS_OK))
		return;

	if ((getLongLongFromObjectOrReply(c, c->argv[2], &ts, NULL) != REDIS_OK))
		return;

	robj *o;
	if ((o = lookupKeyReadOrReply(c, c->argv[3], shared.nokeyerr)) == NULL || checkType(c, o, REDIS_TUAVG)){
		return;
	}

	unique_time_average* ta = (unique_time_average*)o->ptr;

	int bucketDiff = ((ts / bucket_interval) * bucket_interval) - ta->last_updated;
	unsigned int bucketN = (ts / bucket_interval) % TU_BUCKETS;
	unsigned int clear_buckets = bucketDiff / bucket_interval;

	addReplyMultiBulkLen(c, 2);

	if (clear_buckets >= TU_BUCKETS){
		addReplyBulkLongLong(c, 0);
		addReplyBulkLongLong(c, ta->created_time);
		return;
	}

	if (bucketDiff < 0){
		clear_buckets = 0;
	}

	unsigned int num_buckets = TU_BUCKETS - clear_buckets;

	long long sum = 0;
	for (unsigned int i = 0; i<num_buckets; i++){
		unsigned int k = (bucketN + i) % TU_BUCKETS;
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

	addReplyBulkLongLong(c, sum);
	addReplyBulkLongLong(c, ta->created_time);
}

void tuupdateCommand(redisClient *c) {
	long bucket_interval;
	long long ts;

	//the bucket
	if ((getLongFromObjectOrReply(c, c->argv[1], &bucket_interval, NULL) != REDIS_OK))
		return;

	if ((getLongLongFromObjectOrReply(c, c->argv[3], &ts, NULL) != REDIS_OK))
		return;

	uint32_t timestamp = (uint32_t)ts;

	robj* unique = c->argv[2];

	unsigned int bucketN = (ts / bucket_interval) % TU_BUCKETS;

	for (int i = 4; i < c->argc; i++){
		robj *o = tuTypeLookupWriteOrCreate(c, c->argv[i], timestamp);

		if (o == NULL || checkType(c,o,REDIS_TUAVG)) {
			continue;
		}

		unique_time_average* ta = (unique_time_average*)o->ptr;


		//difference between the begining of the previously updated bucket and now.
		//int limits the max time a value can be stale
		int bucketDiff = ((ts / bucket_interval) * bucket_interval) - ta->last_updated;

		if (bucketDiff > bucket_interval){
			unsigned int clear_buckets = bucketDiff / bucket_interval;

			if (clear_buckets >= TU_BUCKETS){
				clear_buckets = TU_BUCKETS;
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

		ta->last_updated = timestamp;

		addReply(c, shared.ok);

		signalModifiedKey(c->db, c->argv[1]);
		notifyKeyspaceEvent(REDIS_NOTIFY_LIST, "tuupdate", c->argv[i], c->db->id);
		server.dirty++;
	}
}
