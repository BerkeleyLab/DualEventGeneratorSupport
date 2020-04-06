/*
 * Shadow the existing MRF-based event generator
 */
program shadowEVG ("SYS=LI11,T=,P=EVG:,R=")

option -c; /* No wait for connections */
option +s; /* Safe mode */

#define SEQUENCE_CAPACITY 2048

/* Old EVG */
unsigned int oldTimestamps[SEQUENCE_CAPACITY];
assign oldTimestamps to "{SYS}:EVG1-SoftSeq:0:Timestamp-SP";
monitor oldTimestamps;

int oldTimestamps[SEQUENCE_CAPACITY];
assign oldTimestamps to "{SYS}:EVG1-SoftSeq:0:EvtCode-SP";
monitor oldTimestamps;

int oldCommitCmd;
assign oldCommitCmd to "{SYS}:EVG1-SoftSeq:0:Commit-Cmd";

int oldEnableCmd;
assign oldEnableCmd to "{SYS}:EVG1-SoftSeq:0:Enable-Cmd";

#ifndef MONITOR_ONLY
/* New EVG */
unsigned int newSequence;
assign newSequence to "{P}{R}E1:SEQ0";

unsigned int newSequenceEnable;
assign newSequenceEnable to "{P}{R}E1:SEQ0:enable";

unsigned int newSingleShot;
assign newSingleShot to "{P}{R}INJ:singleShot";
#endif

ss shadow {
    state awaitingConnections {
        int i = 0;
        when (pvConnectCount() == pvAssignCount()) { } state idle
        delay(5) {
            if (i == 0) {
                printf("waiting for PVs to connect.\n");
                i = 12;
            }
            else {
                i--;
            }
        }
    }

    state idle {
        printf("IDLE\n");
    }
}
