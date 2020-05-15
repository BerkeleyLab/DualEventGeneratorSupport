/**
 * tuningModes.c
 *
 * THIS FILE IS GENERATED - ANY CHANGES WILL BE LOST WHEN THIS IOC IS BUILT
 *
 * Defines functions whose return values depend on the Timing PV Names Google Spreadsheet.
 * Generated from TuningModeTriggers.csv
 */

#include "tuningModes.h"
#include "timingSequenceDefs.h"
#include <stdio.h>

/*
 * Convert target bucket to delay
 */
static int
getTargetBucketDelay(int targetBucket)
{
	return (125 * ((21 * targetBucket) % 328)) / 4;
}

int getTimestamp(unsigned char evtcode, int *syncDelays, int nBunches, int gunBunchesDelay, int targetBucket) {
    switch (evtcode) {
    case 10: return 0 + 0;
    case 12: return syncDelays[INJ_SYNCDELAY_INDEX] + 0;
    case 14: return syncDelays[INJ_SYNCDELAY_INDEX] + 1 + -625000;
    case 15: return syncDelays[INJ_SYNCDELAY_INDEX] + 2 + -625000;
    case 16: return syncDelays[INJ_SYNCDELAY_INDEX] + 3;
    case 18: return syncDelays[INJ_SYNCDELAY_INDEX] + 4;
    case 20: return syncDelays[INJ_SYNCDELAY_INDEX] + 5;
    case 22: return syncDelays[INJ_SYNCDELAY_INDEX] + 6;
    case 24: return syncDelays[INJ_SYNCDELAY_INDEX] + 7;
    case 26: return syncDelays[INJ_SYNCDELAY_INDEX] + 8;
    case 28: return syncDelays[INJ_SYNCDELAY_INDEX] + 9;
    case 30: return syncDelays[INJ_SYNCDELAY_INDEX] + 10;
    case 32: return syncDelays[INJ_SYNCDELAY_INDEX] + 11;
    case 34: return syncDelays[INJ_SYNCDELAY_INDEX] + 12;
    case 36: return syncDelays[INJ_SYNCDELAY_INDEX] + gunBunchesDelay + 13;
    case 37: return syncDelays[INJ_SYNCDELAY_INDEX] + gunBunchesDelay + nBunches + 13;
    case 38: return syncDelays[EXTR_SYNCDELAY_INDEX] + 0;
    case 39: return syncDelays[EXTR_SYNCDELAY_INDEX] + getTargetBucketDelay(targetBucket) + 15 + -650000;
    case 40: return syncDelays[EXTR_SYNCDELAY_INDEX] + getTargetBucketDelay(targetBucket) + 1 + -650000;
    case 42: return syncDelays[EXTR_SYNCDELAY_INDEX] + getTargetBucketDelay(targetBucket) + 2 + -625000;
    case 44: return syncDelays[EXTR_SYNCDELAY_INDEX] + getTargetBucketDelay(targetBucket) + 3;
    case 46: return syncDelays[EXTR_SYNCDELAY_INDEX] + getTargetBucketDelay(targetBucket) + 4;
    case 48: return syncDelays[EXTR_SYNCDELAY_INDEX] + getTargetBucketDelay(targetBucket) + 5;
    case 50: return syncDelays[EXTR_SYNCDELAY_INDEX] + getTargetBucketDelay(targetBucket) + 6;
    case 52: return syncDelays[EXTR_SYNCDELAY_INDEX] + getTargetBucketDelay(targetBucket) + 7;
    case 54: return syncDelays[EXTR_SYNCDELAY_INDEX] + getTargetBucketDelay(targetBucket) + 8;
    case 56: return syncDelays[EXTR_SYNCDELAY_INDEX] + getTargetBucketDelay(targetBucket) + 9;
    case 58: return syncDelays[EXTR_SYNCDELAY_INDEX] + getTargetBucketDelay(targetBucket) + 10;
    case 60: return syncDelays[EXTR_SYNCDELAY_INDEX] + getTargetBucketDelay(targetBucket) + 11;
    case 62: return syncDelays[EXTR_SYNCDELAY_INDEX] + getTargetBucketDelay(targetBucket) + 12;
    case 64: return syncDelays[EXTR_SYNCDELAY_INDEX] + getTargetBucketDelay(targetBucket) + 13;
    case 66: return syncDelays[EXTR_SYNCDELAY_INDEX] + getTargetBucketDelay(targetBucket) + 14;
    case 68: return syncDelays[EXTR_SYNCDELAY_INDEX] + getTargetBucketDelay(targetBucket) + 0 + 2500000;
    case 70: return syncDelays[EXTR_SYNCDELAY_INDEX] + getTargetBucketDelay(targetBucket) + 1 + 2500000;
    case 127: return DELAY_END + 1;
    default:
        printf("Invalid event code %d passed to getTimestamp; returning -1\n", evtcode);
        return -1;
    }
}

unsigned char modeEvtCodes[NUM_MODES][MAX_SEQUENCE_LENGTH] = {
    {
        10,
        12,
        18,
        24,
        26,
        28,
        38,
        39,
        50,
        56,
        70,
        127
    },
    {
        10,
        12,
        14,
        18,
        20,
        22,
        24,
        26,
        28,
        32,
        36,
        37,
        38,
        39,
        50,
        56,
        70,
        127
    },
    {
        10,
        12,
        14,
        15,
        18,
        20,
        22,
        24,
        26,
        28,
        30,
        32,
        34,
        36,
        37,
        38,
        39,
        40,
        46,
        48,
        50,
        56,
        70,
        127
    },
    {
        10,
        12,
        14,
        15,
        18,
        20,
        22,
        24,
        26,
        28,
        30,
        32,
        34,
        36,
        37,
        38,
        39,
        40,
        42,
        44,
        46,
        48,
        50,
        52,
        54,
        56,
        70,
        127
    },
    {
        10,
        12,
        14,
        15,
        16,
        18,
        20,
        22,
        24,
        26,
        28,
        30,
        32,
        34,
        36,
        37,
        38,
        39,
        40,
        42,
        44,
        46,
        48,
        50,
        52,
        54,
        56,
        58,
        60,
        62,
        64,
        66,
        68,
        70,
        127
    },
    {
        10,
        12,
        18,
        24,
        26,
        28,
        38,
        39,
        50,
        56,
        70,
        127
    }
};
