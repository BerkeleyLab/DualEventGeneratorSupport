#ifndef __TUNINGMODES_H__
#define __TUNINGMODES_H__

#include "timingSequenceConstants.h"

int getTimestamp(unsigned char evtcode, int *syncDelays, int nBunches, int gunBunchesDelay, int targetBucket);

extern unsigned char modeEvtCodes[NUM_MODES][MAX_SEQUENCE_LENGTH];

#endif // __TUNINGMODES_H__

