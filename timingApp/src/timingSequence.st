program timingSequence ("SYS=LI11test,T=")

// Required program variables:
//  SYS: crate name e.g. LI11
//  T: "Test" for test crate, "" for production
// Defaults provided here will be overridden by seq command in st.cmd.

// Wait for connections, safe mode
option +cs;
// run-time debug messages
option +d;

#include "timingSequenceConstants.h"
%%#include "timingSequenceDefs.h"
%%#include "timingSequenceHelpers.h"
%%#include "tuningModes.h"
%%#include <alarm.h>
%%#include <dbDefs.h>

// May 2017: Temporary flag during commissioning, since we can't rely on
// mode == DEFAULT_MODE.  This prevents mistaken increments of goodCount.
int8_t default_mode = 0;

// Variables extracted from the request waveform:
/**
 * Sequence number: last element in the request waveform, must change from previous
 * request or request will be ignored.
 */
int32_t sequence = 0;
/**
 * Target bucket number, 1-328, into which we want to inject in the storage ring.
 */
int32_t bucket = 1;
/**
 * Number of bunches from the electron gun, 1-16.
 */
int32_t bunches = 1;
/**
 * Inhibit flag: if non-zero, the electron gun will not be fired even if normally
 * expected for the present mode.  This is to aid in debugging.
 */
int32_t inhibit = 0;
/**
 * Mode: Corresponds to one of the tuning or injection modes.  The event codes to
 * be sent for each mode are defined in modeEvtCodes.
 */
int32_t mode = 0;

/**
 * gunBunchesDelay: the delay in event clock ticks corresponding to the number of
 * bunches to be output by the electron gun.  Obtained from the waveform PV
 * {T}GunBunchToInjFieldTrigger (bunchesToField variable)
 */
int32_t gunBunchesDelay;

/**
 * lastxxx: the previously-loaded request parameters, used to detect when loading the default
 * mode sequence is unecessary
 */
int32_t lastSequence = -1;
int32_t lastBucket = -1;
int32_t lastBunches = -1;
int32_t lastInhibit = -1;
int32_t lastMode = -1;

/**
 * Storage ring current.  The sequencer will refuse to inject when this is higher
 * than MAX_BEAM_CURRENT.
 */
double beamcurrent;
assign beamcurrent to PV_BEAMCURRENT;
monitor beamcurrent;

/**
 * Potentate is the global disable flag for the entire timing system cycle.
 */
uint8_t potentate;
assign potentate to PV_POTENTATE;
monitor potentate;

int syncDelays[3];
assign syncDelays to {
	PV_INJFIELDSYNCDELAY,
	PV_EXTRFIELDSYNCDELAY,
	PV_EVR2DLYGEN6DELAY
};
monitor syncDelays;
evflag syncDelaysFlag;
sync syncDelays syncDelaysFlag;

/**
 * Command to disable the sequencer.  This is turned on when the Potentate
 * is detected to ensure no more cycles will execute until the Potentate
 * is turned back off.
 */
unsigned short disableCmd;
assign disableCmd to PV_DISABLECMD;

/**
 * Enable command pv for the sequencer.  The sequencer operates in single-trigger
 * mode, so this command must be re-sent to initiate every cycle, after the previous
 * cycle has completed.
 */
unsigned short enableCmd;
assign enableCmd to PV_ENABLECMD;

/**
 * Enable input pv.  When the previous cycle completes, this goes to zero, indicating
 * that it is okay to start a new cycle.
 */
unsigned short enableInp;
assign enableInp to PV_ENABLEINP;
monitor enableInp;

/**
 * Count of successful cycles, excluding those of the default cycle.
 */
int32_t goodCount;
assign goodCount to PV_GOODCOUNT;
monitor goodCount;

/**
 * Count of unsuccessful, non-default, cycles.  Possible causes include
 * an invalid request waveform, disconnected pv(s), or timeouts.
 */
int32_t badCount;
assign badCount to PV_BADCOUNT;
monitor badCount;

/**
 * Counts of individual error conditions
 */
int32_t startEvtTimeoutCount, extrEvtTimeoutCount, loadTimeoutCount, eosTimeoutCount;
assign startEvtTimeoutCount to PV_STARTEVTTIMEOUTCOUNT;
assign extrEvtTimeoutCount to PV_EXTREVTTIMEOUTCOUNT;
assign loadTimeoutCount to PV_LOADTIMEOUTCOUNT;
assign eosTimeoutCount to PV_EOSTIMEOUTCOUNT;
int32_t invalidModeCount, invalidBunchesCount, invalidBucketCount;
assign invalidModeCount to PV_INVALIDMODECOUNT;
assign invalidBunchesCount to PV_INVALIDBUNCHESCOUNT;
assign invalidBucketCount to PV_INVALIDBUCKETCOUNT;
int32_t missingEOSCount;
assign missingEOSCount to PV_MISSINGEOSCOUNT;
int32_t waitForConnectCount, waitForSeverityCount;
assign waitForConnectCount to PV_WAITFORCONNECTCOUNT;
assign waitForSeverityCount to PV_WAITFORSEVERITYCOUNT;


/**
 * The request waveform.  See individual variables above for explanations.
 */
int32_t request[NUM_REQUEST_FIELDS];
assign request to PV_REQUEST;
monitor request;
evflag requestFlag;
sync request requestFlag;

/**
 * The electron gun bias, depends on number of gun bunches
 */
double gunBias;
assign gunBias to PV_GUNBIAS;

/**
 * Mapping of # gun bunches to electron gun bias.
 */
double bunchesToBias[MAX_BUNCHES];
assign bunchesToBias to PV_BUNCHESTOBIAS;
monitor bunchesToBias;

/**
 * Mapping of # gun bunches to injection field trigger
 */
double bunchesToField[MAX_BUNCHES];
assign bunchesToField to PV_BUNCHESTOFIELD;
monitor bunchesToField;

/**
 * Booster Sequence Start Trigger.  When this increments,
 * we know the new sequencer cycle has begun.
 */
int32_t startCount, oldStartCount;
assign startCount to PV_STARTCOUNT;
monitor startCount;

/*
 * Extraction Post-Trigger.  After this increments, a new
 * sequence can be loaded into the sequencer.
 */
int32_t postExtrCount, oldPostExtrCount;
assign postExtrCount to PV_POSTEXTRCOUNT;
monitor postExtrCount;

/**
 * Soft sequence event codes.  These are loaded into the EVG
 * for the current mode, on each cycle.
 */
uint8_t evtcodes[MAX_SEQUENCE_LENGTH];
assign evtcodes to PV_EVTCODES;

/**
 * Soft sequence timestamps, in event clock ticks.
 * These are loaded into the EVG for the current mode, on each cycle.
 */
int tstamps[MAX_SEQUENCE_LENGTH];
assign tstamps to PV_TSTAMPS;

/**
 * Commit command to transfer the loaded event code/timestamp pairs
 * into the MRF hardware.  Must be issued each time a new sequence
 * is loaded.
 */
uint16_t commitCmd;
assign commitCmd to PV_COMMITCMD;

/**
 * Status strings: a descriptive status string for debugging, and
 * the current state in which the sequencer is
 */
string mystatus;
assign mystatus to PV_MYSTATUS;
uint32_t mystate;
assign mystate to PV_MYSTATE;

/**
 * Monitor heartbeat pvs on remote EVR iocs, so we know to enter
 * wait_for_connected if any of those crates goes away
 */
uint8_t b0215link, s0123link, s0817link;
assign b0215link to PV_B0215LINK;
assign s0123link to PV_S0123LINK;
assign s0817link to PV_S0817LINK;

entry {
	modeIndexes[DEFAULT_MODE] = DEFAULT_MODE_INDEX;
	modeIndexes[LINACTUNE_MODE] = LINACTUNE_MODE_INDEX;
	modeIndexes[BRTUNE_MODE] = BRTUNE_MODE_INDEX;
	modeIndexes[BTSTUNE_MODE] = BTSTUNE_MODE_INDEX;
	modeIndexes[SRINJECTION_MODE] = SRINJECTION_MODE_INDEX;
	modeIndexes[SRINJECTION_PREPARE_MODE] = SRINJECTION_PREPARE_MODE_INDEX;

	initArrays(syncDelays);
}

ss ssTiming {

	/*
	 * Load the default sequence
	 */
	state default_sequence {

		entry {
			mystate = DEFAULT_SEQUENCE;
			pvPut(mystate);
		}

		when () {
			// April 2017: For temporary commissioning, SRINJECTION has been
			// redefined as the "default" mode
			//mode = DEFAULT_MODE;
			mode = SRINJECTION_MODE;
			// May 2017: Also don't force gun inhibit during commissioning
			//inhibit = 1;
			// May 2017: Set the default_mode flag, since we can't rely on
			// mode == DEFAULT_MODE.  This can be removed after commissioning
			default_mode = 1;
		} state load_requested_sequence

	}

	/*
	 * Wait for sequence to end, then start the next sequence
	 */
	state wait_for_eos {
		option -t;

		entry {
			mystate = WAIT_FOR_EOS;
			pvPut(mystate);
		}

		when (potentate == 0) {
		} state wait_for_potentate

		when (!enableInp) {
			enableCmd = 1;
			pvPut(enableCmd);
		} state wait_for_start

		when (delay(MAX_START_DELAY)) {
			//fprintf(stderr, "wait_for_eos: no EOS in %g seconds; returning to default sequence\n", MAX_START_DELAY);
			++eosTimeoutCount;
			pvPut(eosTimeoutCount);
		} state default_sequence
	}

	/*
	 * Wait for sequence start event
	 */
	state wait_for_start {

		entry {
			mystate = WAIT_FOR_START;
			pvPut(mystate);
		}

		when (oldStartCount != startCount) {
			oldStartCount = startCount;
		} state wait_for_extraction

		when (delay(MAX_START_DELAY)) {
			fprintf(stderr, "wait_for_start: no start detected in %g seconds.  oldStartCount %d startCount %d\n", MAX_START_DELAY, oldStartCount, startCount);
			++startEvtTimeoutCount;
			pvPut(startEvtTimeoutCount);
		} state increment_bad_count

	}

	/*
	 * Wait for post-extraction event
	 */
	state wait_for_extraction {
		entry {
			mystate = WAIT_FOR_EXTRACTION;
			pvPut(mystate);
		}

		when (oldPostExtrCount != postExtrCount) {
			oldPostExtrCount = postExtrCount;
		} state wait_for_request

		when (delay(MAX_EXTRACTION_DELAY)) {
			fprintf(stderr, "wait_for_extraction: sequence started but no post extraction event detected\n");
			++extrEvtTimeoutCount;
			pvPut(extrEvtTimeoutCount);
		} state increment_bad_count

	}

	/*
	 * Open time window for injection request 
	 */
	state wait_for_request {
		entry {
			mystate = WAIT_FOR_REQUEST;
			pvPut(mystate);
		}

		when (delay(REQUEST_WINDOW)) {
		} state process_request

	}
	
	/*
	 * Check for injection request 
	*/
	state process_request {

		entry {
			mystate = PROCESS_REQUEST;
			pvPut(mystate);
		}

		when (efTestAndClear(requestFlag)) {
			if (sequence == request[SEQUENCE]) {
				/* No request, or perhaps it was truncated */
				state default_sequence;
			}

			bucket   = request[TARGET_BUCKET];
			bunches  = request[GUN_BUNCHES];
			mode     = request[INJ_MODE];
			inhibit  = request[GUN_INHIBIT];
			sequence = request[SEQUENCE];
		
			// May 2017: clear the default_mode flag, since we can't rely on
			// mode == DEFAULT_MODE during commissioning
			default_mode = 0;

			//printf("bucket %d bunches %d mode %d inhibit %d sequence %d\n",
			//	bucket, bunches, mode, inhibit, sequence);

		} state validate_request

		when (!efTest(requestFlag)) {
			strcpy(mystatus, "no new request");
		} state default_sequence
	}

	state validate_request {

		entry {
			mystate = VALIDATE_REQUEST;
			pvPut(mystate);
		}

		when ( mode < MIN_MODE || mode > MAX_MODE ) {
			fprintf(stderr, "Invalid mode %d, aborting injection\n", mode);
			++invalidModeCount;
			pvPut(invalidModeCount);
		} state increment_bad_count

		when ( bucket < MIN_BUCKETS ||  bucket > MAX_BUCKETS ) {
			fprintf(stderr, "Invalid target bucket %d, aborting injection\n", bucket);
			++invalidBucketCount;
			pvPut(invalidBucketCount);
		} state increment_bad_count

		when ( bunches < MIN_BUNCHES || bunches > MAX_BUNCHES ) {
			fprintf(stderr, "Invalid #bunches %d, aborting injection\n", bunches);
			++invalidBunchesCount;
			pvPut(invalidBunchesCount);
		} state increment_bad_count

		when () {
		} state check_beamcurrent
	}

	state check_beamcurrent {

		entry {
			mystate = CHECK_BEAMCURRENT;
			pvPut(mystate);
		}

		when ((mode == SRINJECTION_MODE) && (pvGet(beamcurrent, SYNC, 0.01) != pvStatOK)) {
			fprintf(stderr, "Unable to retrieve beam current; cancelling SRINJECTION request\n");
		} state increment_bad_count

		when (mode == SRINJECTION_MODE && (pvSeverity(beamcurrent) == pvSevrINVALID)) {
			sprintf(mystatus, "%s.SEVR = INVALID; cancelling SRINJECTION request", PV_BEAMCURRENT);
			pvPut(mystatus);
			fprintf(stderr, "%s\n", mystatus);
		} state increment_bad_count

		when (mode == SRINJECTION_MODE && (beamcurrent > MAX_BEAM_CURRENT)) {
			sprintf(mystatus, "%s %.02f exceeds maximum %.02f", PV_BEAMCURRENT, beamcurrent, MAX_BEAM_CURRENT);
			pvPut(mystatus);
		} state increment_bad_count

		when () {
		} state check_severities
	}

	state check_severities {

		entry {
			mystate = CHECK_SEVERITIES;
			pvPut(mystate);
		}

		when (pvSeverity(disableCmd) != pvSevrNONE) {
			fprintf(stderr, "%s.SEVR is not NONE; cancelling request\n", PV_DISABLECMD);
		} state increment_bad_count

		when (pvSeverity(enableCmd) != pvSevrNONE) {
			fprintf(stderr, "%s.SEVR is not NONE; cancelling request\n", PV_ENABLECMD);
		} state increment_bad_count

		when (pvSeverity(enableInp) != pvSevrNONE) {
			fprintf(stderr, "%s.SEVR is not NONE; cancelling request\n", PV_ENABLEINP);
		} state increment_bad_count

		when (pvSeverity(startCount) != pvSevrNONE) {
			fprintf(stderr, "%s.SEVR is not NONE; cancelling request\n", PV_STARTCOUNT);
		} state increment_bad_count

		when (pvSeverity(postExtrCount) != pvSevrNONE) {
			fprintf(stderr, "%s.SEVR is not NONE; cancelling request\n", PV_POSTEXTRCOUNT);
		} state increment_bad_count

		when () {
		} state check_fiber_links
	}

	state check_fiber_links {

		entry {
			mystate = CHECK_FIBER_LINKS;
			pvPut(mystate);
		}

		when (pvGet(b0215link, SYNC, 0.01) != pvStatOK) {
			fprintf(stderr, "Failed to get %s; cancelling request\n", PV_B0215LINK);
		} state increment_bad_count

		when (pvGet(s0123link, SYNC, 0.01) != pvStatOK) {
			fprintf(stderr, "Failed to get %s; cancelling request\n", PV_S0123LINK);
		} state increment_bad_count

		when (pvGet(s0817link, SYNC, 0.01) != pvStatOK) {
			fprintf(stderr, "Failed to get %s; cancelling request\n", PV_S0817LINK);
		} state increment_bad_count

		when (!b0215link || !s0123link || !s0817link) {
			fprintf(stderr, "Lost Fiber Link; cancelling request\n");
		} state increment_bad_count

		when (b0215link && s0123link && s0817link) {
		} state load_requested_sequence
	}

	/*
	 * Load the requested sequence into the EVG
	 */
	state load_requested_sequence {
		option +e;
		option -t;

		uint8_t haveTStamps;
		uint8_t sortedTStamps;
		int32_t nTStamps;

		entry {
			//printf("load_requested_sequence entry\n");
			haveTStamps = 0;
			sortedTStamps = 0;
			mystate = LOAD_REQUESTED_SEQUENCE;
			pvPut(mystate);
			gunBunchesDelay = bunchesToField[bunches-1];
			gunBias = bunchesToBias[bunches-1];
			// Apr. 2017: while in an intermediate commissioning state,
			// topoff code sets gun bias
			//pvPut(gunBias);
		}

		when (delay(LOAD_WINDOW)) {
			/* shot parameter load didn't complete - shot failed */
			fprintf(stderr, "timeout in load_requested_sequence\n");
			++loadTimeoutCount;
			pvPut(loadTimeoutCount);
		} state increment_bad_count

		/* Removed, because it causes confusion in commissioning.  May
			be added back at some time.
		when (mode == DEFAULT_MODE && lastMode == DEFAULT_MODE
			&& lastSequence == sequence
			&& lastBucket == bucket
			&& lastBunches == bunches
			&& lastInhibit == inhibit) {
			// Still in default mode: don't bother loading array
		} state wait_for_eos
		*/

		when (!haveTStamps && efTestAndClear(syncDelaysFlag)) {
			initArrays(syncDelays);
		} state load_requested_sequence

		when (!haveTStamps && mode >= MIN_MODE && mode <= MAX_MODE) {
			uint8_t * e = modeEvtCodes[modeIndexes[mode]];
			int32_t i, j;
			uint8_t foundEOS = 0;

			//printf("Mode = %d; modeIndexes[%d] = %lu\n", mode, mode, modeIndexes[mode]);
			memset(evtcodes, 0, sizeof(evtcodes[0])*MAX_SEQUENCE_LENGTH);
			memset(tstamps, 0, sizeof(tstamps[0])*MAX_SEQUENCE_LENGTH);
			for (i = 0, j = 0; i < MAX_SEQUENCE_LENGTH; ++i) {
				//printf("[%u] e: %hu t: %g\n", i, e[i], t[i]);
				if (inhibit) {
					if (e[i] == GUNON_EVTCODE || e[i] == GUNOFF_EVTCODE) {
						continue;
					}
				}
				evtcodes[j] = e[i];
				tstamps[j] = getTimestamp(e[i], syncDelays, bunches, gunBunchesDelay, bucket);
				/*if (mode != DEFAULT_MODE) {
					printf("[%u] evtcode: %hu tstamp: %d\n", j, evtcodes[j], tstamps[j]);
				}*/
				if (tstamps[j] < 0) {
					fprintf(stderr, "WARNING: tstamp for evtcode %hu is %d; forcing to zero\n", evtcodes[j], tstamps[j]);
					tstamps[j] = 0;
				}
				if (e[i] == SEQUENCE_END_EVTCODE) {
					foundEOS = 1;
					break;
				}
				++j;
			}

			if (!foundEOS) {
				fprintf(stderr, "No EOS event in mode %d array\n", mode);
				++missingEOSCount;
				pvPut(missingEOSCount);
				state default_sequence;
			//} else {
			//	printf("%u event(s) found in mode %d array\n", i, mode);
			}
			nTStamps = j;
			haveTStamps = 1;
		} state load_requested_sequence

		when (haveTStamps && !sortedTStamps) {
			//int i;
			//printf("Sorting %d tstamps\n", nTStamps);
			/*for (i = 0; i < nTStamps; ++i) {
				printf("%g ", tstamps[i]);
			}
			printf("\n");*/
			quickSort(evtcodes, tstamps, 0, nTStamps-1);
			/*printf("After sort:\n");
			for (i = 0; i < nTStamps; ++i) {
				printf("%g ", tstamps[i]);
			}
			printf("\n");*/
			uniqueTimestamps(tstamps, nTStamps);
			sortedTStamps = 1;
		} state load_requested_sequence

		when (sortedTStamps) {
			//printf("Sending %d tstamps\n", nTStamps);
			pvPut(evtcodes);
			pvPut(tstamps);
			commitCmd = 1;
			pvPut(commitCmd);

			//printf("Updating lastMode from %d to %d\n", lastMode, mode);
			lastMode = mode;
			lastSequence = sequence;
			lastBucket = bucket;
			lastBunches = bunches;
			lastInhibit = inhibit;

			// May 2017: use default_mode flag instead of mode == DEFAULT_MODE
			// during commissioning
			if (!default_mode) {
			//if (mode > DEFAULT_MODE) {
				++goodCount;
				//printf("Incrementing good count to %d\n", goodCount);
				pvPut(goodCount);
			}

		} state wait_for_eos

	}

	/*
	 * Wait indefinitely for potentate enable
	 */
	state wait_for_potentate {

		entry {
			lastMode = -1;
			disableCmd = 1;
			pvPut(disableCmd);
			mystate = WAIT_FOR_POTENTATE;
			pvPut(mystate);
		}

		when (!pvConnected(potentate)) {
		} state wait_for_potentate

		when (potentate == 1) {
			// Discard any requests that may have come in
			efClear(requestFlag);
		} state default_sequence

	}

	state increment_bad_count {

		entry {
			mystate = INCREMENT_BAD_COUNT;
			pvPut(mystate);
		}

		when() {
			++badCount;
			pvPut(badCount);
		} state default_sequence

	}


} /* ss timing */

