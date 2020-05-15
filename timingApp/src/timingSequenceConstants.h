#ifndef __TIMINGSEQUENCECONSTANTS_H__
#define __TIMINGSEQUENCECONSTANTS_H__

#define MAX_SEQUENCE_LENGTH   100
#define EVG_SEQUENCE_CAPACITY 200

// Some ranges for request parameters
#define MIN_BUCKETS 1
#define MAX_BUCKETS 328
#define MIN_BUNCHES 1
#define MAX_BUNCHES 16

// Delay windows (seconds)
#define REQUEST_WINDOW 0.5
#define LOAD_WINDOW 0.2

#define NUM_MODES 6
#define MIN_MODE DEFAULT_MODE
#define MAX_MODE SRINJECTION_PREPARE_MODE

#define NUM_EVTCODES                255
#define GUNON_EVTCODE               36
#define GUNOFF_EVTCODE              37
#define SEQUENCE_END_EVTCODE        127

// Delays used in calculating evt code timestamps
// Units are approx. 8 nsec ticks
#define DELAY_GUNON           10000
#define DELAY_END             100000000

// Indexes into the syncDelays array
#define INJ_SYNCDELAY_INDEX     0
#define EXTR_SYNCDELAY_INDEX    1

// Upper limit on allowable beam current (mA)
#define MAX_BEAM_CURRENT        510.0

#endif // __TIMINGSEQUENCECONSTANTS_H__

