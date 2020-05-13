program timingSequence ("T=T.P=EVG1:,R=")

// Macros
//  T: "Test" for test crate, "" for production
//  P:, R: Event generator prefixes
// Defaults provided here can be overridden by seq command in st.cmd.

// Wait for connections, safe mode
option +cs;
// run-time debug messages
option +d;

#include "timingSequenceConstants.h"
%%#include "timingSequenceDefs.h"
%%#include "timingSequenceHelpers.h"
%%#include "tuningModes.h"

/*
 * Injection request waveform.
 */
int32_t request[NUM_REQUEST_FIELDS];
assign request to "{T}TimInjReq";
monitor request;
evflag requestFlag;
sync request requestFlag;

/* Values extracted from injection request waveform: */
int sequence = 0;   /* Sequence #: request ignored if no change from previous */
int lastSequence = -1;
int bucket = 1;     /* Target bucket (1-328) */
int bunches = 1;    /* Number of electron gun bunches (1-16) */
int inhibit = 0;    /* Inhibit electron gun if set, even if mode allows gun */
int mode = 0;       /* Type of injection sequence.  Indexes modeEvtCodes. */

/*
 * Storage ring current.
 * Sequencer refuses to inject when this exceeds MAX_BEAM_CURRENT.
 */
double beamcurrent;
assign beamcurrent to "SR:DCCT:AVG";
monitor beamcurrent;

/*
 * Overall timing system enable/disable
 */
int potentate;
assign potentate to "{T}TimPotentate";
monitor potentate;
int injCycleEnable;
assign injCycleEnable to "{P}{R}INJ:injCycleEnable";

/*
 * Field delays
 */
int syncDelays[2];
assign syncDelays to {
    "{T}TimInjFieldSyncDelay",
    "{T}TimExtrFieldSyncDelay"
};
monitor syncDelays;
evflag syncDelaysFlag;
sync syncDelays syncDelaysFlag;

/*
 * Statistics
 */
int startOverrunCount;
assign startOverrunCount to "{T}TimSeqStartOverrunCount";
int staleSequenceCount;
assign staleSequenceCount to "{T}TimSeqStaleSequenceCount";
int invalidModeCount;
assign invalidModeCount to "{T}TimSeqInvalidModeCount";
int invalidBucketCount;
assign invalidBucketCount to "{T}TimSeqInvalidBucketCount";
int invalidBunchesCount;
assign invalidBunchesCount to "{T}TimSeqInvalidBunchesCount";
int noBeamCurrentCount;
assign noBeamCurrentCount to "{T}TimSeqNoBeamCurrentCount";
int badBeamCurrentCount;
assign badBeamCurrentCount to "{T}TimSeqBadBeamCurrentCount";
int excessiveBeamCurrentCount;
assign excessiveBeamCurrentCount to "{T}TimExcessiveBeamCurrentCount";
int badSeverityCount;
assign badSeverityCount to "{T}TimSeqBadSeverityCount";
int noFiberStatusCount;
assign noFiberStatusCount to "{T}TimSeqNoFiberStatusCount";
int badFiberStatusCount;
assign badFiberStatusCount to "{T}TimSeqBadFiberStatusCount";
int syncDelayTimeoutCount;
assign syncDelayTimeoutCount to "{T}TimSeqSyncDelayTimeoutCount";
int goodCount;
assign goodCount to "{T}TimSeqSuccessCount";
int badCount;
assign badCount to "{T}TimSeqFailedCount";

/*
 * Diagnostics
 */
int debugFlag;
assign debugFlag to "{T}TimSeqDebug";
monitor debugFlag;

/**
 * Mapping of # gun bunches to injection field trigger
 */
double bunchesToField[MAX_BUNCHES];
assign bunchesToField to "{T}GunBunchToInjFieldTrigger";
monitor bunchesToField;
int gunBunchesDelay;

/*
 * Soft sequence
 */
uint8_t evtcodes[MAX_SEQUENCE_LENGTH];
int tstamps[MAX_SEQUENCE_LENGTH];

/*
 * EVR IOC records.
 * Injection cycles are initiated only when all are valid.
 * FIXME: Should LI11:EVR:[12], CR:EVR1, and S0220:EVR1 also be checked?
 */
int b0215link, s0123link, s0817link;
assign b0215link to "B0215:EVR1:Link-Sts";
assign s0123link to "S0123:EVR1:Link-Sts";
assign s0817link to "S0817:EVR1:Link-Sts";

/*
 * Event generator FPGA
 */
#define EVG_SEQ_STATUS_ACTIVE 0x8
#define EVG_SEQ_STATUS_BUSY   0x10
int evgSequence[EVG_SEQUENCE_CAPACITY];
assign evgSequence to "{P}{R}E1:SEQ1";
int evgSeqEnable;
assign evgSeqEnable to "{P}{R}E1:SEQ1:enable";
int evgSeqStatus;
assign evgSeqStatus to "{P}{R}E1:seqStatus";
monitor evgSeqStatus;
int evgSequencerStarts;
assign evgSequencerStarts to "{P}{R}E1:seqStarts";
monitor evgSequencerStarts;
int evgSequencerCurrentStarts;

entry {
    modeIndexes[DEFAULT_MODE] = DEFAULT_MODE_INDEX;
    modeIndexes[LINACTUNE_MODE] = LINACTUNE_MODE_INDEX;
    modeIndexes[BRTUNE_MODE] = BRTUNE_MODE_INDEX;
    modeIndexes[BTSTUNE_MODE] = BTSTUNE_MODE_INDEX;
    modeIndexes[SRINJECTION_MODE] = SRINJECTION_MODE_INDEX;
    modeIndexes[SRINJECTION_PREPARE_MODE] = SRINJECTION_PREPARE_MODE_INDEX;
}

ss ssTiming {
    /*
     * Initial state
     * Wait for injection cycles to be enabled
     */
    state await_potentate {
        entry {
            if(debugFlag) printf("State: await_potentate\n");
            injCycleEnable = 0;
            pvPut(injCycleEnable, SYNC);
        }
        when (potentate != 0) {
            injCycleEnable = 1;
            pvPut(injCycleEnable, SYNC);
        } state sync_await_start
    }

    /*
     * Initial synchronization
     */
state sync_await_start {
        entry { if(debugFlag) printf("State: sync_await_start\n"); }
        when (potentate == 0) { } state await_potentate

        when ((evgSeqStatus & EVG_SEQ_STATUS_ACTIVE) != 0) {
        } state await_completion
    }

    /*
     * Wait for injection cycle to start
     */
    state await_start {
        entry { if(debugFlag) printf("State: await_start\n"); }
        when (potentate == 0) { } state await_potentate

        when ((evgSeqStatus & EVG_SEQ_STATUS_ACTIVE) != 0) {
            efClear(syncDelaysFlag);
        } state await_precompletion
    }

    /*
     * Wait for injection cycle to reach precompletion point
     */
    state await_precompletion {
        entry { if(debugFlag) printf("State: await_precompletion\n"); }
        when (potentate == 0) { } state await_potentate

        when (efTestAndClear(requestFlag)) {
            /* Ignore requests outside expected window */
        } state await_precompletion

        when ((evgSeqStatus & EVG_SEQ_STATUS_BUSY) == 0) {
            evgSequencerCurrentStarts = evgSequencerStarts;
        } state await_request
    }

    /*
     * Wait for injection cycle to complete
     */
    state await_completion {
        entry { if(debugFlag) printf("State: await_completion\n"); }
        when (potentate == 0) { } state await_potentate

        when ((evgSeqStatus & EVG_SEQ_STATUS_ACTIVE) == 0) {
        } state await_start
    }

    /*
     * Watch for injection request
     */
    state await_request {
        entry { if(debugFlag) printf("State: await_request\n"); }
        when (potentate == 0) { } state await_potentate

        when (delay(REQUEST_WINDOW)) {
            /* No request, default cycle will be emitted */
        } state await_completion

        when (evgSequencerStarts != evgSequencerCurrentStarts) {
            startOverrunCount++;
            pvPut(startOverrunCount);
        } state increment_bad_count

        when (efTestAndClear(requestFlag)) {
            if (sequence == request[SEQUENCE]) {
                /* No request, or perhaps it was truncated */
                state await_completion;
            }
            bucket   = request[TARGET_BUCKET];
            bunches  = request[GUN_BUNCHES];
            mode     = request[INJ_MODE];
            inhibit  = request[GUN_INHIBIT];
            sequence = request[SEQUENCE];
        } state validate_request
    }

    /*
     * Transient sequential states to validate, prepare, and send sequence
     */
    state validate_request {
        entry { if(debugFlag) printf("State: validate_request\n"); }
        when (sequence == lastSequence) {
            staleSequenceCount++;
            pvPut(staleSequenceCount);
        } state increment_bad_count

        when ((mode < MIN_MODE) || (mode > MAX_MODE)) {
            invalidModeCount++;
            pvPut(invalidModeCount);
        } state increment_bad_count

        when ((bucket < MIN_BUCKETS) || (bucket > MAX_BUCKETS)) {
            fprintf(stderr, "Invalid target bucket %d, aborting injection\n", bucket);
            invalidBucketCount++;
            pvPut(invalidBucketCount);
        } state increment_bad_count

        when ((bunches < MIN_BUNCHES) || (bunches > MAX_BUNCHES)) {
            fprintf(stderr, "Invalid #bunches %d, aborting injection\n", bunches);
            invalidBunchesCount++;
            pvPut(invalidBunchesCount);
        } state increment_bad_count

        when () {
        } state check_beamcurrent
    }

    state check_beamcurrent {
        entry { if(debugFlag) printf("State: check_beamcurrent\n"); }
        when ((mode == SRINJECTION_MODE) && (pvGet(beamcurrent, SYNC, 0.01) != pvStatOK)) {
            noBeamCurrentCount++;
            pvPut(noBeamCurrentCount);
        } state increment_bad_count

        when ((mode == SRINJECTION_MODE) && (pvSeverity(beamcurrent) == pvSevrINVALID)) {
            badBeamCurrentCount++;
            pvPut(badBeamCurrentCount);
        } state increment_bad_count

        when ((mode == SRINJECTION_MODE) && (beamcurrent > MAX_BEAM_CURRENT)) {
            excessiveBeamCurrentCount++;
            pvPut(excessiveBeamCurrentCount);
        } state increment_bad_count

        when () {
        } state check_severities
    }

    state check_severities {
        entry { if(debugFlag) printf("State: check_severities\n"); }
        when ((pvSeverity(evgSeqStatus) != pvSevrNONE)
            || (pvSeverity(evgSequencerStarts) != pvSevrNONE)) {
            badSeverityCount++;
            pvPut(badSeverityCount);
        } state increment_bad_count

        when () {
        } state check_fiber_links
    }

    state check_fiber_links {
        entry { if(debugFlag) printf("State: check_fiber_links\n"); }
        when ((pvGet(b0215link, SYNC, 0.01) != pvStatOK)
           || (pvGet(s0123link, SYNC, 0.01) != pvStatOK)
           || (pvGet(s0123link, SYNC, 0.01) != pvStatOK)) {
            noFiberStatusCount++;
            pvPut(noFiberStatusCount);
        } state increment_bad_count

        when (!b0215link || !s0123link || !s0817link) {
            badFiberStatusCount++;
            pvPut(badFiberStatusCount);
        } state increment_bad_count

        when () {
        } state awaitSyncs
    }

    /*
     * Wait for field-related adjustments
     */
    state awaitSyncs {
        entry { if(debugFlag) printf("State: awaitSyncs\n"); }
        when (efTestAndClear(syncDelaysFlag)) {
        } state create_requested_sequence

        when (delay(LOAD_WINDOW)) {
            syncDelayTimeoutCount++;
            pvPut(syncDelayTimeoutCount);
        } state increment_bad_count
    }

    /*
     * Create the new sequence
     */
    state create_requested_sequence {
        entry { if(debugFlag) printf("State: create_requested_sequence\n"); }
        when () {
            uint8_t * e = modeEvtCodes[modeIndexes[mode]];
            int32_t i, nTStamps;
            uint32_t highwater = 0, highwaterCount = 0;
            uint8_t foundEOS = 0;

            gunBunchesDelay = bunchesToField[bunches-1];
            for (i = 0, nTStamps = 0; i < MAX_SEQUENCE_LENGTH; i++) {
                if (inhibit) {
                    if (e[i] == GUNON_EVTCODE || e[i] == GUNOFF_EVTCODE) {
                        continue;
                    }
                }
                evtcodes[nTStamps] = e[i];
                tstamps[nTStamps] = getTimestamp(e[i], syncDelays, bunches, gunBunchesDelay, bucket);
                if (tstamps[nTStamps] < 0) {
                    fprintf(stderr, "WARNING: tstamp for evtcode %hu is %d; forcing to zero\n", evtcodes[nTStamps], tstamps[nTStamps]);
                    tstamps[nTStamps] = 0;
                }
                if (e[i] < SEQUENCE_END_EVTCODE) {
                    if (tstamps[nTStamps] > highwater) {
                        highwater = tstamps[nTStamps];
                    }
                }
                else {
                    tstamps[nTStamps] = highwater + ++highwaterCount;
                }
                nTStamps++;
                if (e[i] == SEQUENCE_END_EVTCODE) {
                    foundEOS = 1;
                    break;
                }
            }
            if (!foundEOS) {
                fprintf(stderr, "No EOS event in mode %d\n", mode);
                state await_completion;
            }
            quickSort(evtcodes, tstamps, 0, nTStamps-1);
            uniqueTimestamps(evtcodes, tstamps, nTStamps, evgSequence);
        } state load_requested_sequence
    }

    state load_requested_sequence {
        entry { if(debugFlag) printf("State: load_requested_sequence\n"); }
        when (potentate == 0) { } state await_potentate

        when (evgSequencerStarts != evgSequencerCurrentStarts) {
            startOverrunCount++;
            pvPut(startOverrunCount);
        } state increment_bad_count

        when ((evgSeqStatus & EVG_SEQ_STATUS_ACTIVE) == 0) {
            pvPut(evgSequence);
            evgSeqEnable = 1;
            pvPut(evgSeqEnable);
            lastSequence = sequence;
            goodCount++;
            pvPut(goodCount);
        } state await_start
    }

    state increment_bad_count {
        entry { if(debugFlag) printf("State: increment_bad_count\n"); }
        when() {
            badCount++;
            pvPut(badCount);
        } state await_completion
    }
}
