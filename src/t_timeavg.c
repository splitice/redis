#include "redis.h"
#include <math.h>
#include "timeavg.h"

robj *createTaObject(void) {
	unsigned char *zl = zcalloc(sizeof(time_average));
	robj *o = createObject(REDIS_TAVG, zl);
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

void tahitCommand(redisClient *c) {
	robj *o = taTypeLookupWriteOrCreate(c, c->argv[1]);
	long bucket_interval, by, ts;

	//the bucket
	if ((getLongFromObjectOrReply(c, c->argv[2], &bucket_interval, NULL) != REDIS_OK))
		return; 

	if ((getLongFromObjectOrReply(c, c->argv[3], &by, NULL) != REDIS_OK))
		return;

	if ((getLongFromObjectOrReply(c, c->argv[4], &ts, NULL) != REDIS_OK))
		return;

	if (o == NULL || checkType(c, o, REDIS_TAVG)) {
		return;
	}

	unsigned int bucketN = (ts / bucket_interval) % TA_BUCKETS;

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
	notifyKeyspaceEvent(REDIS_NOTIFY_LIST, "tahit", c->argv[1], c->db->id);
	server.dirty++;

	unsigned long sum = 0;
	for (unsigned int i = 0; i < TA_BUCKETS; i++){
		sum += ta->buckets[i];
	}

	addReplyLongLong(c, sum);
}

void tacalcCommand(redisClient *c){
	robj *o;
	if ((o = lookupKeyReadOrReply(c, c->argv[1], shared.nokeyerr)) == NULL || checkType(c, o, REDIS_TAVG)){
		return;
	}

	long bucket_interval, ts;

	if ((getLongFromObjectOrReply(c, c->argv[2], &bucket_interval, NULL) != REDIS_OK))
		return;

	if ((getLongFromObjectOrReply(c, c->argv[3], &ts, NULL) != REDIS_OK))
		return;

	time_average* ta = (time_average*)o->ptr;

	int updated_ago = ((ts / bucket_interval) * bucket_interval) - ta->last_updated;
	
	unsigned long sum = 0;
	if (updated_ago < 0){
		for (unsigned int i = 0; i < TA_BUCKETS; i++){
			sum += ta->buckets[i];
		}
	}
	else{
		unsigned int clear_buckets = updated_ago / bucket_interval;

		//If we need to clear all buckets, then the value will be 0
		if (clear_buckets < TA_BUCKETS){
			unsigned int bucketN = (ts / bucket_interval) % TA_BUCKETS;
			for (unsigned int i = clear_buckets; i < TA_BUCKETS; i++){
				unsigned int k = (bucketN - i) % TA_BUCKETS;
				sum += ta->buckets[k];
			}
		}
	}
	
	addReplyLongLong(c, sum);
}