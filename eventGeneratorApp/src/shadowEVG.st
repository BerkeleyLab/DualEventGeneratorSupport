/*
 * Shadow the existing MRF-based event generator
 */
program shadowEVG("SYS=LI11,T=,P=EVG,R=:")

option -c; /* No wait for connections */
option +s; /* Safe mode */

%%#include <math.h>
#define UPDATE_STATUS(...) snprintf(statusMsg, sizeof statusMsg, __VA_ARGS__), \
                           pvPut(statusMsg)
#define DIAGNOSTIC(...) if (debugLevel > 0) printf(__VA_ARGS__)

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
assign oldEnableRB to "{SYS}:EVG1-SoftSeq:0:Enable-RB.VAL";
monitor oldEnableRB;
syncq oldEnableRB 2;

/*
 * Detect updates
 * It might seem more reasonable for the main state set to monitor
 * "{SYS}:EVG1-SoftSeq:0:Commit-Cmd" to indicate when to push the
 * sequence to the FPGA, but that record  does not post monitors
 * each time it processes since its value does not change.
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
int missedTriggerCount;
assign missedTriggerCount to "{P}{R}shadowEVG:missedTriggers";
evflag enableSoftTrigger;

ss shadow {
    state awaitingConnections {
        int i = 0;
        entry {
            UPDATE_STATUS("Started");
        }
        when (pvConnectCount() == pvAssignCount()) {
        } state starting
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

    /* Wait for first sequence update to finish */
    state starting {
        when (efTest(oldTimestampsUpdated) && efTest(oldEventCodesUpdated)) {
            efClear(oldTimestampsUpdated);
            efClear(oldEventCodesUpdated);
        } state idle
    }

    /* Wait for new sequence */
    state idle {
        entry {
            efClear(enableSoftTrigger);
            newSequenceEnable = 0;
            pvPut(newSequenceEnable, SYNC);
        }
        when (efTest(oldTimestampsUpdated) || efTest(oldEventCodesUpdated)) {
        } state updateSequenceInfo
    }

    /* Timestamps or event codes have changed */
    state updateSequenceInfo {
        when (efTest(oldTimestampsUpdated) && efTest(oldEventCodesUpdated)) {
            efClear(oldTimestampsUpdated);
            efClear(oldEventCodesUpdated);
        } state convertSequence
        when (delay(0.3)) {
            UPDATE_STATUS("Partial sequence update!\n");
        } state idle
    }

    /* Timestamps and event codes have changed */
    state convertSequence {
        when (TRUE) {
            int i, j = 0, limit;
            uint32_t previousTimestamp = -1;
            limit = pvCount(oldTimestamps); 
            if (limit > SEQUENCE_CAPACITY) limit = SEQUENCE_CAPACITY;
            for (i = 0 ; i < limit ; i++) {
                int thisTimestamp = oldTimestamps[i];
                int thisEventCode = oldEventCodes[i];
                int gap = thisTimestamp - previousTimestamp - 1;
                /*
                 * No need to pad sequence to the event marking the full
                 * injection interval, so bail out when that code is seen.
                 */
                if (thisEventCode == EVENT_CODE_FINAL) {
                    gap = 0;
                    thisEventCode = EVENT_CODE_TERMINATOR;
                }
                if (j >= (NEW_SEQUENCE_SIZE - 1)) {
                    UPDATE_STATUS("No room for event!");
                    state idle;
                }
                DIAGNOSTIC("%d %d %d\n", thisTimestamp, thisEventCode, gap);
                newSequence[j++] = gap;
                newSequence[j++] = thisEventCode;
                previousTimestamp = thisTimestamp;
                if (thisEventCode == EVENT_CODE_TERMINATOR) {
                    state haveSequence;
                }
            }
            UPDATE_STATUS("No terminating event!");
        } state idle
    }

    /* Valid sequence received */
    state haveSequence {
        entry {
            newSequenceEnable = 0;
            pvPut(newSequenceEnable, SYNC);
            pvPut(newSequence, SYNC);
            newSequenceEnable = 1;
            pvPut(newSequenceEnable, SYNC);
            efSet(enableSoftTrigger);
            DIAGNOSTIC("Updated\n");
        }
        when (efTest(oldTimestampsUpdated) || efTest(oldEventCodesUpdated)) {
        } state updateSequenceInfo
        when (delay(1.4)) {
            DIAGNOSTIC("No new sequence.\n");
        } state idle
    }
}

ss trigger {
    /*
     * Wait for current sequence finish
     */
    state awaitCompletion {
        when (pvGetQ(oldEnableRB) && !oldEnableRB) {
        } state awaitTrigger
    }

    /*
     * Wait for new sequence to be triggered.
     */
    state awaitTrigger {
        when (pvGetQ(oldEnableRB) && oldEnableRB) {
            if (efTestAndClear(enableSoftTrigger)) {
                pvPut(newSingleShot, SYNC);
                DIAGNOSTIC("Triggered\n");
            }
            else {
                missedTriggerCount++;
                pvPut(missedTriggerCount);
            }
        } state awaitCompletion
    }
}
