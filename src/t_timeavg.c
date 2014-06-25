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
	unsigned char *zl = zcalloc(sizeof(unique_time_average));
	unique_time_average* zl_uta = (unique_time_average*)zl;
	robj *o = createObject(REDIS_TUAVG, zl);
	for (int i = 0; i < TU_BUCKETS; i++){
		zl_uta->buckets[i] = createHLLObject();
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
				sdsfree(r->ptr);
				zfree(r);
				ta->buckets[k] = createHLLObject();
			}
		}

		hllAdd(ta->buckets[bucketN], (unsigned char*)c->argv[2]->ptr, sdslen(c->argv[2]->ptr));

		ta->last_updated = (unsigned int)ts;


		long long sum = 0;
		for (unsigned int i = 0; i < TU_BUCKETS; i++){
			robj* r = ta->buckets[i];
			struct hllhdr *hdr;
			uint64_t card;

			hdr = o->ptr;
			if (HLL_VALID_CACHE(hdr)) {
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
			else {
				int invalid = 0;
				/* Recompute it and update the cached value. */
				card = hllCount(hdr, &invalid);
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
				/* This is not considered a read-only command even if the
				* data structure is not modified, since the cached value
				* may be modified and given that the HLL is a Redis string
				* we need to propagate the change. */

				sum += *(uint64_t*)hdr->card;
			}
		}

		signalModifiedKey(c->db, c->argv[1]);
		notifyKeyspaceEvent(REDIS_NOTIFY_LIST, "tuhit", c->argv[i], c->db->id);
		server.dirty++;

		addReplyBulkLongLong(c, sum);
	}
}

void tucalcCommand(redisClient *c){
	/*
	long bucket_interval, ts;

	if ((getLongFromObjectOrReply(c, c->argv[1], &bucket_interval, NULL) != REDIS_OK))
		return;

	if ((getLongFromObjectOrReply(c, c->argv[2], &ts, NULL) != REDIS_OK))
		return;

	robj *o;
	if ((o = lookupKeyReadOrReply(c, c->argv[3], shared.nokeyerr)) == NULL || checkType(c, o, REDIS_TAVG)){
		return;
	}

	unique_time_average* ta = (unique_time_average*)o->ptr;

	int updated_ago = ((ts / bucket_interval) * bucket_interval) - ta->last_updated;

	long long sum = 0;
	if (updated_ago < 0){
		for (unsigned int i = 0; i < TU_BUCKETS; i++){
			sum += ta->buckets[i];
		}
	}
	else{
		unsigned int clear_buckets = updated_ago / bucket_interval;

		//If we need to clear all buckets, then the value will be 0
		if (clear_buckets < TU_BUCKETS){
			unsigned int bucketN = (ts / bucket_interval) % TU_BUCKETS;
			for (; clear_buckets < TU_BUCKETS; clear_buckets++){
				unsigned int k = (bucketN - clear_buckets) % TU_BUCKETS;
				sum += ta->buckets[k];
			}
		}
	}

	addReplyLongLong(c, sum);*/
}