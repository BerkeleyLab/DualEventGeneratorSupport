#ifndef __TIMINGSEQUENCEHELPERS_H__
#define __TIMINGSEQUENCEHELPERS_H__

/**
 * Fields in the request waveform.  See individual variables below for explanations.
 */
typedef enum {
	TARGET_BUCKET,
	GUN_BUNCHES,
	INJ_MODE,
	GUN_INHIBIT,
	RESERVED1,
	RESERVED2,
	SEQUENCE
} REQUEST_FIELDS;

/**
 * Array of indexes into the modeEvtCodes array.  Array index
 * is the mode number (e.g. 40 for SR Injection); the value is
 * the index in modeEvtCodes (e.g. 4 for SR Injection).
 */
unsigned int modeIndexes[255];

/**
 * Array of timestamps for all event codes.  Index is the
 * event code (e.g. 18 for Gun On), value is the base timestamp
 * (i.e. before adding gun bias or target bucket delay).
 */
int all_evtcode_tstamps[255];

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the modeIndexes and all_evt_tstamps arrays
 */
void initArrays(int syncDelays[]);

/**
 * Sort the event code & timestamp arrays in order of increasing timstamp
 */
void quickSort(unsigned char * evtcodes, int * tstamps, int left, int right);

/**
 * Force the timstamps array to unique values.  Assumes the array has already
 * been sorted, and tstamps are integers.
 */
void uniqueTimestamps(int * tstamps, int count);

/**
 * Calculate the delay offset for the given target bucket number (1-328)
 */
int getTargetBucketDelay(int targetBucket);

#ifdef __cplusplus
} // extern "C"
#endif

#endif //  __TIMINGSEQUENCEHELPERS_H__

