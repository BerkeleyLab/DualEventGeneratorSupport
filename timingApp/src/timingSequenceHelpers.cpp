#include "timingSequenceHelpers.h"
#include "timingSequenceDefs.h"
#include <stdio.h>

/*******************************
 * Function Implementations
 */

extern double psReadyDelay;

extern "C" {

void quickSort(unsigned char * evtcodes, int * tstamps, int left, int right) {
	int i = left, j = right;
	//int k;
	int t;
	unsigned char e;
	int pivot = tstamps[(left+right)/2];

	//printf("quickSort(evtcodes, tstamps, %ld, %ld)\n", left, right);
	//for (k = left; k <= right; ++k) {
//		printf("%g ", tstamps[k]);
//	}
	//printf("\n");
	
	// partition
	while (i <= j) {
		while (tstamps[i] < pivot)
			++i;
		while (tstamps[j] > pivot)
			--j;
		if (i <= j) {
			//printf("swapping %ld and %ld\n", i, j);
			t = tstamps[i];
			tstamps[i] = tstamps[j];
			tstamps[j] = t;
			e = evtcodes[i];
			evtcodes[i] = evtcodes[j];
			evtcodes[j] = e;
			++i;
			--j;
		}
	}

	// recursion
	if (left < j) {
		//printf("quickSort: recursing left=%ld, j=%ld\n", left, j);
		quickSort(evtcodes, tstamps, left, j);
	}
	if (i < right) {
		//printf("quickSort: recursing i=%ld, right=%ld\n", i, right);
		quickSort(evtcodes, tstamps, i, right);
	}

	//printf("quickSort(evtcodes, tstamps, %ld, %ld): done\n", left, right);
}

void
uniqueTimestamps(unsigned char *evtcodes, int *tstamps, int count, int *merged) {
	int i;
    int prev = -1;
	for (i = 0; i < count-1; ++i) {
		if (tstamps[i+1] <= tstamps[i]) {
			tstamps[i+1] = tstamps[i] + 1;
		}
        *merged++ = (tstamps[i] - prev) - 1;
        prev = tstamps[i];
        *merged++ = evtcodes[i];
	}
}

int getTargetBucketDelay(int targetBucket) {
	return (125 * ((21 * targetBucket) % 328)) / 4;
}

} // extern "C"

