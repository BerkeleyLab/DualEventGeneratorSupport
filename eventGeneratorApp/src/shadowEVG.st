/*
 * Shadow the existing MRF-based event generator
 */
program shadowEVG("SYS=LI11,T=,P=EVG:,R=")

option -c; /* No wait for connections */
option +s; /* Safe mode */

%%#include <math.h>
#define UPDATE_STATUS(...) snprintf(statusMsg, sizeof statusMsg, __VA_ARGS__), \
                           pvPut(statusMsg)

#define SEQUENCE_CAPACITY       2048
#define EVENT_CODE_TERMINATOR   127
#define EVENT_CODE_FINAL        100

/* Reduce latency by keeping new sequence size to a reasonble value */
#define NEW_SEQUENCE_SIZE 100

/* Old EVG */
unsigned int oldTimestamps[SEQUENCE_CAPACITY];
assign oldTimestamps to "{SYS}:EVG1-SoftSeq:0:Timestamp-SP";
monitor oldTimestamps;

int oldEventCodes[SEQUENCE_CAPACITY];
assign oldEventCodes to "{SYS}:EVG1-SoftSeq:0:EvtCode-SP";
monitor oldEventCodes;

int oldEnableRB;
assign oldEnableRB to "{SYS}:EVG1-SoftSeq:0:Enable-RB";
monitor oldEnableRB;

/*
 * Detect updates
 * It might seem more reasonable for the main state set to monitor
 * "{SYS}:EVG1-SoftSeq:0:Commit-Cmd" but that record  does not post
 * monitors each time it processes since its value does not change.
 */
evflag oldTimestampsUpdated;
sync oldTimestamps to oldTimestampsUpdated;
evflag oldEventCodesUpdated;
sync oldEventCodes to oldEventCodesUpdated;

/* New EVG */
unsigned int newSequence[NEW_SEQUENCE_SIZE];
assign newSequence to "{P}{R}E1:SEQ0";

unsigned int newSequenceEnable;
assign newSequenceEnable to "{P}{R}E1:SEQ0:enable";

unsigned int newSingleShot;
assign newSingleShot to "{P}{R}INJ:singleShot";

/* Status */
string statusMsg;
assign statusMsg to "{P}{R}shadowEVG:msg";
int debugLevel;
assign debugLevel "{P}{R}shadowEVG:diag";
monitor debugLevel;

ss shadow {
    state awaitingConnections {
        int i = 0;
        entry {
            UPDATE_STATUS("Started");
        }
        when (pvConnectCount() == pvAssignCount()) { } state idle
        when (delay(5)) {
            if (i == 0) {
                UPDATE_STATUS("Waiting for %d of %d PVs to connect.",
                           pvAssignCount() - pvConnectCount(), pvAssignCount());
                i = 5;
            }
            else {
                i--;
            }
        } state awaitingConnections
    }

    /* Wait for new sequence */
    state idle {
        when (efTestAndClear(oldTimestampsUpdated)
           || efTestAndClear(oldEventCodesUpdated)) { } state validateSequence
        when (delay(1.2)) {
            newSequenceEnable = 0;
            pvPut(newSequenceEnable, SYNC);
            if (debugLevel > 0) {
                printf("Timed out waiting for new sequence.\n");
            }
        } state idle
    }

    /* Update FPGA when time stamps and event codes are consistent */
    state validateSequence {
        when (TRUE) {
            struct epicsTimeStamp tsTime = pvTimeStamp(oldTimestamps);
            struct epicsTimeStamp evTime = pvTimeStamp(oldEventCodes);
            struct epicsTimeStamp now;
            epicsTimeGetCurrent(&now);
            if (pvCount(oldTimestamps) == pvCount(oldEventCodes)) {
                double d1 = fabs(epicsTimeDiffInSeconds(&tsTime, &evTime));
                double d2 = fabs(epicsTimeDiffInSeconds(&now, &evTime));
                if (debugLevel > 0) {
                    printf("Stale:%g mismatched:%g\n", d2, d1);
                }
                if ((d1 < 0.4) && (d2 < 0.5)) {
                    int i, j = 0, limit;
                    uint32_t previousTimestamp = -1;
                    limit = pvCount(oldTimestamps); 
                    if (limit > SEQUENCE_CAPACITY) limit = SEQUENCE_CAPACITY;
                    for (i = 0 ; (i < limit) && (j < NEW_SEQUENCE_SIZE) ; i++) {
                        int thisTimestamp = oldTimestamps[i];
                        int thisEventCode = oldEventCodes[i];
                        int gap = thisTimestamp - previousTimestamp - 1;
                        if (thisEventCode == EVENT_CODE_FINAL) {
                            gap = 0;
                            thisEventCode = EVENT_CODE_TERMINATOR;
                        }
                        newSequence[j++] = gap;
                        newSequence[j++] = thisEventCode;
                        previousTimestamp = thisTimestamp;
                        if (debugLevel > 0) {
                            printf("%d: %d %d  (%d)\n", i, thisTimestamp,
                                               thisEventCode, newSequence[j-2]);
                        }
                        if (thisEventCode == EVENT_CODE_TERMINATOR) {
                            if (debugLevel > 0) {
                                printf("DONE AFTER %d %d\n", i, j);
                            }
                            pvPut(newSequence, SYNC);
                            newSequenceEnable = 1;
                            pvPut(newSequenceEnable, SYNC);
                        }
                    }
                    UPDATE_STATUS("No terminating event!");
                }
            }
            else {
                UPDATE_STATUS("Timestamp(%d)/eventCode(%d) pvCount mismatch!",
                                pvCount(oldTimestamps), pvCount(oldEventCodes));
            }
        } state idle
    }
}

ss trigger {
    /*
     * Wait for current sequence finish
     */
    state awaitCompletion {
        when (!oldEnableRB) {
        } state awaitTrigger
    }

    /*
     * Wait for new sequence to be triggered.
     */
    state awaitTrigger {
        when (oldEnableRB) {
            pvPut(newSingleShot, SYNC);
        } state awaitTrigger
    }
}
