#ifndef __TIMINGSEQUENCECONSTANTS_H__
#define __TIMINGSEQUENCECONSTANTS_H__


/**
 * PV Names:
 */
#define PV_BEAMCURRENT "SR:DCCT:AVG"
#define PV_POTENTATE "{T}TimPotentate"
// syncDelays array[3]:
#define PV_INJFIELDSYNCDELAY  "{T}TimInjFieldSyncDelay"
#define PV_EXTRFIELDSYNCDELAY "{T}TimExtrFieldSyncDelay"
#define PV_EVR2DLYGEN6DELAY   "{SYS}:EVR2-DlyGen:6:Delay-SP"
#define PV_DISABLECMD "{SYS}:EVG1-SoftSeq:0:Disable-Cmd"
#define PV_ENABLECMD "{SYS}:EVG1-SoftSeq:0:Enable-Cmd"
#define PV_ENABLEINP "{SYS}:EVG1-SoftSeq:0:Enable-RB"
#define PV_GOODCOUNT "{T}TimSeqSuccessCount"
#define PV_BADCOUNT "{T}TimSeqFailedCount"
#define PV_STARTEVTTIMEOUTCOUNT "{T}TimSeqStartEventTimeoutCount"
#define PV_EXTREVTTIMEOUTCOUNT "{T}TimSeqExtrEventTimeoutCount"
#define PV_LOADTIMEOUTCOUNT "{T}TimSeqLoadTimeoutCount"
#define PV_EOSTIMEOUTCOUNT "{T}TimSeqEOSTimeoutCount"
#define PV_INVALIDMODECOUNT "{T}TimSeqInvalidModeCount"
#define PV_INVALIDBUNCHESCOUNT "{T}TimSeqInvalidBunchesCount"
#define PV_INVALIDBUCKETCOUNT "{T}TimSeqInvalidBucketCount"
#define PV_MISSINGEOSCOUNT "{T}TimSeqMissingEOSCount"
#define PV_WAITFORCONNECTCOUNT "{T}TimSeqWaitForConnectCount"
#define PV_WAITFORSEVERITYCOUNT "{T}TimSeqWaitForSeverityCount"
#define PV_REQUEST "{T}TimInjReq"
#define PV_GUNBIAS "{T}EG______BIAS___AC01"
#define PV_BUNCHESTOBIAS "{T}GunBunchToGunBias"
#define PV_BUNCHESTOFIELD "{T}GunBunchToInjFieldTrigger"
#define PV_STARTCOUNT "{SYS}:EVR1:Evt10Cnt-I"
#define PV_POSTEXTRCOUNT "{SYS}:EVR1:Evt38Cnt-I"
#define PV_EVTCODES "{SYS}:EVG1-SoftSeq:0:EvtCode-SP"
#define PV_TSTAMPS "{SYS}:EVG1-SoftSeq:0:Timestamp-SP"
#define PV_COMMITCMD "{SYS}:EVG1-SoftSeq:0:Commit-Cmd"
#define PV_MYSTATUS "{T}TimSeqStatus"
#define PV_MYSTATE "{T}TimSeqState"
#define PV_B0215LINK "B0215:EVR1:Link-Sts"
#define PV_S0123LINK "S0123:EVR1:Link-Sts"
#define PV_S0817LINK "S0817:EVR1:Link-Sts"

#define MAX_SEQUENCE_LENGTH 2047

#define NUM_REQUEST_FIELDS 7

// Some ranges for request parameters
#define MIN_BUCKETS 1
#define MAX_BUCKETS 328
#define MIN_BUNCHES 1
#define MAX_BUNCHES 16

// Delay windows (seconds)
#define MAX_START_DELAY 1.4
#define MAX_EXTRACTION_DELAY 1.0
#define REQUEST_WINDOW 0.5
#define LOAD_WINDOW 0.2

#define NUM_MODES 6
#define MIN_MODE DEFAULT_MODE
#define MAX_MODE SRINJECTION_PREPARE_MODE

#define NUM_EVTCODES                255
#define START_EVTCODE               10
#define GTBCCD_EVTCODE              14
#define INJFIELD_MIN_EVTCODE        16
#define GUNON_EVTCODE               36
#define GUNOFF_EVTCODE              37
#define EXTRFIELD_MIN_EVTCODE       38
#define BRBUMP_OLD_EVTCODE          39
#define BRBUMP_EVTCODE              40
#define TARGETBUCKET_MIN_EVTCODE    42
#define TARGETBUCKET_MAX_EVTCODE    66
#define PSREADY_EVTCODE             70
#define SEQUENCE_END_EVTCODE        127

#define BRBUMP_DELAY_OFFSET         650000

// Delays used in calculating evt code timestamps
// Units are approx. 8 nsec ticks
#define DELAY_START           0
#define DELAY_GUNON           10000
#define DELAY_END             168740000 // approx. 1.35 s -- must be less than MAX_START_DELAY

// Indexes into the syncDelays array
#define INJ_SYNCDELAY_INDEX     0
#define EXTR_SYNCDELAY_INDEX    1
#define PSREADY_SYNCDELAY_INDEX 2

// Upper limit on allowable beam current (mA)
#define MAX_BEAM_CURRENT        510.0

#endif // __TIMINGSEQUENCECONSTANTS_H__

