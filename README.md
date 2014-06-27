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