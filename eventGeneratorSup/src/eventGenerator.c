/*
 * ALSU event generator device support
 */
#include <string.h>
#include <epicsStdio.h>
#include <epicsString.h>
#include <epicsEvent.h>
#include <epicsExit.h>
#include <epicsThread.h>
#include <epicsTime.h>
#include <epicsMutex.h>
#include <epicsExport.h>
#include <epicsMath.h>
#include <cantProceed.h>
#include <errlog.h>
#include <iocsh.h>
#include <drvAsynIPPort.h>
#include <asynCommonSyncIO.h>
#include <asynOctetSyncIO.h>
#include <asynStandardInterfaces.h>
#include "evgProtocol.h"

/*
 * Local ASYN subaddress
 * Augments values in evgProtocol.h
 */
#define A_HI_IOC                0xF000
# define A_IOC_LO_STATISTICS        0x000

/*
 * Number of times to retry a command
 */
#define COMMAND_RETRY_LIMIT 4

/*
 * Sequencer status subscription
 */
#define SEQUENCER_STATUS_RESUBSCRIBE_SECONDS 10

/*
 * Links to lower port
 */
typedef struct portLink {
    char             *portName;
    char             *hostInfo;
    asynUser         *pasynUserCommon;
    asynUser         *pasynUserOctet;
    int               isCommunicating;
} portLink;

/*
 * Driver private storage
 */
typedef struct drvPvt {
    /*
     * Links to lower-level port drivers
     */
    portLink        cmdLink;
    portLink        seqLink;

    /*
     * Asyn interfaces we provide
     */
    char           *portName;
    char           *threadName;
    asynUser       *pasynUser;  /* For controlling diagnostic messages */
    asynStandardInterfaces asynInterfaces;

    /*
     * I/O buffers
     */
    struct evgPacket commandPacket;
    struct evgPacket replyPacket;

    /*
     * Statistics
     */
    unsigned long   commandCount[COMMAND_RETRY_LIMIT+1];
    unsigned long   commandFailedCount;
    unsigned long   seqConnectCount;
    unsigned long   seqReceivedCount;
    unsigned long   seqMissedCount;
} drvPvt;

/*
 * Arrange for cleanup on IOC shutdown
 */
static volatile int shutdown;
static void
atExitHandler(void *arg)
{
    shutdown = 1;
}

/*
 * asynCommon methods
 */
static void
showLink(FILE *fp, struct portLink *link)
{
    fprintf(fp, " %s %scommunicating\n", link->portName,
                                link->isCommunicating ? "" : "NOT ");
}
static void
report(void *pvt, FILE *fp, int details)
{
    drvPvt *pdpvt = (drvPvt *)pvt;
    showLink(fp, &pdpvt->cmdLink);
    showLink(fp, &pdpvt->seqLink);
}

static asynStatus
connect(void *pvt, asynUser *pasynUser)
{
    pasynManager->exceptionConnect(pasynUser);
    return asynSuccess;
}

static asynStatus
disconnect(void *pvt, asynUser *pasynUser)
{
    pasynManager->exceptionDisconnect(pasynUser);
    return asynSuccess;
}
static asynCommon commonMethods = { report, connect, disconnect };

/*
 * All communication with the FPGA takes place through here
 */
static asynStatus
cmdWriteRead(drvPvt *pdpvt, asynUser *pasynUser, int cmdArgCount, int *replyArgCount)
{
    int retryCount;
    double retryInterval = 0.1;
    size_t cmdSize = EVG_PROTOCOL_ARG_COUNT_TO_SIZE(cmdArgCount);
    size_t nTrans;
    int eom;
    int omitSend = 0;
    asynStatus status;

    pdpvt->commandPacket.magic = EVG_PROTOCOL_MAGIC;
    if (pdpvt->commandPacket.nonce == 0) {
        epicsTimeStamp now;
        epicsTimeGetCurrent(&now);
        pdpvt->commandPacket.nonce = now.secPastEpoch;
    }
    else {
        pdpvt->commandPacket.nonce++;
    }
    if (!pdpvt->cmdLink.isCommunicating) {
        status = pasynCommonSyncIO->connectDevice(pdpvt->cmdLink.pasynUserCommon);
        if (status != asynSuccess) {
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                                    "%s failed to connect to %s: %s\n",
                                        pdpvt->cmdLink.portName,
                                        pdpvt->cmdLink.hostInfo,
                                        pdpvt->cmdLink.pasynUserOctet->errorMessage);
            pdpvt->commandFailedCount++;
            return status;
        }
    }
    pdpvt->cmdLink.pasynUserOctet->timeout = 0.3;
    for (retryCount = 0 ; ; ) {
        if (omitSend) {
            status = asynSuccess;
            omitSend = 0;
        }
        else {
            status = pasynOctetSyncIO->write(pdpvt->cmdLink.pasynUserOctet,
                                               (char *)&pdpvt->commandPacket,
                                               cmdSize, retryInterval, &nTrans);
        }
        if (status == asynSuccess) {
            status = pasynOctetSyncIO->read(pdpvt->cmdLink.pasynUserOctet,
                                                  (char *)&pdpvt->replyPacket,
                                                  sizeof pdpvt->replyPacket,
                                                  retryInterval, &nTrans, &eom);
        }
        if (status == asynSuccess) {
            if (nTrans >= EVG_PROTOCOL_ARG_COUNT_TO_SIZE(0)) {
                if ((pdpvt->replyPacket.nonce == pdpvt->commandPacket.nonce)
                 && (pdpvt->replyPacket.magic == pdpvt->commandPacket.magic)) {
                    pdpvt->cmdLink.isCommunicating = 1;
                    pdpvt->commandCount[retryCount]++;
                    if (replyArgCount != NULL) {
                        *replyArgCount = EVG_PROTOCOL_SIZE_TO_ARG_COUNT(nTrans);
                    }
                    return asynSuccess;
                }
                epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                            "Sent command %08x:%08X, got reply %08X:%08X",
                                       (unsigned int)pdpvt->commandPacket.magic,
                                       (unsigned int)pdpvt->commandPacket.nonce,
                                       (unsigned int)pdpvt->replyPacket.magic,
                                       (unsigned int)pdpvt->replyPacket.nonce);
                status = asynError;
                omitSend = 1;
            }
            else {
                epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                               "Reply packet length %u, need at least %u",
                               (unsigned int)nTrans,
                               (unsigned int)EVG_PROTOCOL_ARG_COUNT_TO_SIZE(0));
                status = asynError;
            }
        }
        if (++retryCount > COMMAND_RETRY_LIMIT)
            break;
        retryInterval = 0.5;
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, "%s retry: %s\n",
                   pdpvt->cmdLink.portName,
                   status == asynTimeout ? "Timeout" : pasynUser->errorMessage);
        pdpvt->cmdLink.pasynUserOctet->timeout = 2.0;
    }
    if (status == asynTimeout)
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                                                                    "Timeout");
    if (pasynCommonSyncIO->disconnectDevice(pdpvt->cmdLink.pasynUserCommon) != asynSuccess)
        errlogPrintf("%s can't disconnect: %s\n",
                                pdpvt->cmdLink.portName,
                                pdpvt->cmdLink.pasynUserCommon->errorMessage);
    pdpvt->cmdLink.isCommunicating = 0;
    pdpvt->commandFailedCount++;
    return status;
}

/*
 * Push monitor data into records
 */
static void
processMonitorPacket(drvPvt *pdpvt, asynStatus status, int replyArgCount)
{
    ELLLIST *pclientList;
    interruptNode *pnode;
    epicsTimeStamp now;

    if ((status == asynSuccess) && (replyArgCount < 2)) {
        status = asynError;
    }
    epicsTimeGetCurrent(&now);
    pasynManager->interruptStart(pdpvt->asynInterfaces.int32InterruptPvt, &pclientList);
    pnode = (interruptNode *)ellFirst(pclientList);
    while (pnode) {
        asynInt32Interrupt *int32Interrupt = pnode->drvPvt;
        int ahi, alo, idx;
        pnode = (interruptNode *)ellNext(&pnode->node);
        ahi = int32Interrupt->addr & EVG_PROTOCOL_CMD_MASK_HI;
        alo = int32Interrupt->addr & EVG_PROTOCOL_CMD_MASK_LO;
        idx = int32Interrupt->addr & EVG_PROTOCOL_CMD_MASK_IDX;
        if (ahi == EVG_PROTOCOL_CMD_HI_SYSMON) {
            int32_t v = 0;
            int32Interrupt->pasynUser->auxStatus = status;
            int32Interrupt->pasynUser->timestamp = now;
            if (status == asynSuccess) {
                if ((idx < 1) || (idx >= replyArgCount)) {
                    int32Interrupt->pasynUser->auxStatus = asynError;
                }
                else if (alo == EVG_PROTOCOL_CMD_SYSMON_LO_INT32) {
                    v = pdpvt->replyPacket.args[idx];
                    }
                else if (alo == EVG_PROTOCOL_CMD_SYSMON_LO_UINT16_LO) {
                    v = pdpvt->replyPacket.args[idx] & 0xFFFF;
                }
                else if (alo == EVG_PROTOCOL_CMD_SYSMON_LO_UINT16_HI) {
                    v = (pdpvt->replyPacket.args[idx] >> 16) & 0xFFFF;
                }
                else if (alo == EVG_PROTOCOL_CMD_SYSMON_LO_INT16_LO) {
                    v = pdpvt->replyPacket.args[idx] & 0xFFFF;
                    if (v & 0x8000) v -= 0x10000;
                }
                else if (alo == EVG_PROTOCOL_CMD_SYSMON_LO_INT16_HI) {
                    v = (pdpvt->replyPacket.args[idx] >> 16) & 0xFFFF;
                    if (v & 0x8000) v -= 0x10000;
                }
                else {
                    int32Interrupt->pasynUser->auxStatus = asynError;
                }
            }
            int32Interrupt->callback(int32Interrupt->userPvt,
                                     int32Interrupt->pasynUser, v);
        }
    }
    pasynManager->interruptEnd(pdpvt->asynInterfaces.int32InterruptPvt);
}

/*
 * asynInt32 methods
 */
static asynStatus
int32Write(void *pvt, asynUser *pasynUser, epicsInt32 value)
{
    drvPvt *pdpvt = (drvPvt *)pvt;
    asynStatus status;
    int address, ahi;
    int replyCount;

    if ((status = pasynManager->getAddr(pasynUser, &address)) != asynSuccess)
        return status;
    ahi = address & EVG_PROTOCOL_CMD_MASK_HI;
    if (ahi == EVG_PROTOCOL_CMD_HI_LONGOUT) {
        pdpvt->commandPacket.command = address;
        pdpvt->commandPacket.args[0] = value;
        status = cmdWriteRead(pdpvt, pasynUser, 1, &replyCount);
    }
    else {
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                                                       "Invalid write address");
        status =  asynError;
    }
    return status;
}

static asynStatus
int32Read(void *pvt, asynUser *pasynUser, epicsInt32 *value)
{
    drvPvt *pdpvt = (drvPvt *)pvt;
    asynStatus status;
    int address, ahi, alo, idx;
    int nRead;

    if ((status = pasynManager->getAddr(pasynUser, &address)) != asynSuccess)
        return status;
    ahi = address & EVG_PROTOCOL_CMD_MASK_HI;
    alo = address & EVG_PROTOCOL_CMD_MASK_LO;
    idx = address & EVG_PROTOCOL_CMD_MASK_IDX;
    switch (ahi) {
    case EVG_PROTOCOL_CMD_HI_SYSMON:
        pdpvt->commandPacket.command = address;
        status = cmdWriteRead(pdpvt, pasynUser, 0, &nRead);
        processMonitorPacket(pdpvt, status, nRead);
        if ((status == asynSuccess) && (nRead < 1)) {
            status = asynError;
        }
        if (status == asynSuccess) {
            *value = pdpvt->replyPacket.args[0];
        }
        break;

    case EVG_PROTOCOL_CMD_HI_LONGIN:
        pdpvt->commandPacket.command = address;
        status = cmdWriteRead(pdpvt, pasynUser, 0, &nRead);
        if ((status == asynSuccess) && (nRead != 1)) {
            epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                                                               "Bad read size");
            status = asynError;
            break;
        }
        *value = pdpvt->replyPacket.args[0];
        break;

    case A_HI_IOC:
        if (alo == A_IOC_LO_STATISTICS) {
            switch (idx) {
            default:
                if (idx <= COMMAND_RETRY_LIMIT) {
                    *value = pdpvt->commandCount[idx];
                }
                break;
            case COMMAND_RETRY_LIMIT + 1:
                *value = pdpvt->commandFailedCount;
                break;
            case COMMAND_RETRY_LIMIT + 2:
                *value = pdpvt->seqConnectCount;
                break;
            case COMMAND_RETRY_LIMIT + 3:
                *value = pdpvt->seqReceivedCount;
                break;
            case COMMAND_RETRY_LIMIT + 4:
                *value = pdpvt->seqMissedCount;
                break;
            }
            break;
        }
        /* Fall through to default case */

    default:
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                                                        "Invalid read address");
        return asynError;
    }
    return status;
}
static asynInt32 int32Methods = { int32Write, int32Read };

/*
 * asynUInt32Digital methods
 */
static asynStatus
uint32DigitalWrite(void *pvt, asynUser *pasynUser, epicsUInt32 value,
                                                               epicsUInt32 mask)
{
    return int32Write(pvt, pasynUser, value & mask);
}
static asynStatus
uint32DigitalRead(void *pvt, asynUser *pasynUser, epicsUInt32 *value,
                                                               epicsUInt32 mask)
{
    asynStatus status;
    epicsInt32 v = 0;

    if ((status = int32Read(pvt, pasynUser, &v)) == asynSuccess) {
        *value = v & mask;
    }
    return status;
}
static asynUInt32Digital uint32DigitalMethods = { uint32DigitalWrite,
                                                  uint32DigitalRead };

/*
 * asynInt32Array methods
 */
static asynStatus
int32ArrayWrite(void *pvt, asynUser *pasynUser, epicsInt32 *value, size_t n)
{
    drvPvt *pdpvt = (drvPvt *)pvt;
    asynStatus status;
    int address, aHi, aLo;
    int replyCount;

    if ((status = pasynManager->getAddr(pasynUser, &address)) != asynSuccess)
        return status;
    aHi = address & EVG_PROTOCOL_CMD_MASK_HI;
    aLo = address & EVG_PROTOCOL_CMD_MASK_LO;
    if ((aHi == EVG_PROTOCOL_CMD_HI_WAVEFORM)
    &&  (aLo == EVG_PROTOCOL_CMD_WAVEFORM_LO_SEQUENCE)) {
        int nSend = 1;
        int pkNumber = 0;
        if ((n < 2) || (n % 2)) {
            epicsSnprintf(pasynUser->errorMessage,
                               pasynUser->errorMessageSize, "Invalid table size");
            return asynError;
        }
        pdpvt->commandPacket.command = address;
        while (n) {
            uint32_t delay = *value++;
            int code = *value++ & 0xFF;
            n -= 2;
            if (delay < EVG_PROTOCOL_WAVEFORM_SINGLE_WORD_DELAY_LIMIT) {
                pdpvt->commandPacket.args[nSend++] = (delay << 8) | code;
            }
            else {
                pdpvt->commandPacket.args[nSend++] = code |
                           (EVG_PROTOCOL_WAVEFORM_SINGLE_WORD_DELAY_LIMIT << 8);
                pdpvt->commandPacket.args[nSend++] = delay;
            }
            if (code == EVG_PROTOCOL_WAVEFORM_END_OF_TABLE_EVENT_CODE) {
                n = 0;
            }
            if ((n == 0)
             && (code != EVG_PROTOCOL_WAVEFORM_END_OF_TABLE_EVENT_CODE)) {
                epicsSnprintf(pasynUser->errorMessage,
                             pasynUser->errorMessageSize, "No end-of-sequence");
                return asynError;
            }
            if ((n == 0)
             || (nSend == EVG_PROTOCOL_ARG_CAPACITY)
             || ((nSend == (EVG_PROTOCOL_ARG_CAPACITY-1))
              && (*value >= EVG_PROTOCOL_WAVEFORM_SINGLE_WORD_DELAY_LIMIT))) {
                pdpvt->commandPacket.args[0] = pkNumber;
                status = cmdWriteRead(pdpvt, pasynUser, nSend, &replyCount);
                if (status != asynSuccess) {
                    return status;
                }
                nSend = 1;
                pkNumber++;
            }
        }
    }
    else {
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                                                             "Invalid address");
        return asynError;
    }
    return asynSuccess;
}

static asynStatus
int32ArrayRead(void *pvt, asynUser *pasynUser, epicsInt32 *value, size_t n, size_t *nIn)
{
    asynStatus status;
    int address, ahi;

    if ((status = pasynManager->getAddr(pasynUser, &address)) != asynSuccess)
        return status;
    ahi = address & EVG_PROTOCOL_CMD_MASK_HI;
    switch (ahi) {
    default:
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                                                             "Invalid address");
        return asynError;
    }
    return asynSuccess;
}
static asynInt32Array int32ArrayMethods = { int32ArrayWrite, int32ArrayRead };

/*
 * asynOctet methods
 */
static asynStatus
octetRead(void *pvt, asynUser *pasynUser, char *data, size_t maxchars, size_t *nTransferred, int *eomReason)
{
    drvPvt *pdpvt = (drvPvt *)pvt;
    asynStatus status;
    int address, nRead;
    int len;
    time_t tBuf;

    if ((status = pasynManager->getAddr(pasynUser, &address)) != asynSuccess)
        return status;
    switch (address) {
    case EVG_PROTOCOL_CMD_HI_LONGIN|EVG_PROTOCOL_CMD_LONGIN_IDX_FIRMWARE_BUILD_DATE:
    case EVG_PROTOCOL_CMD_HI_LONGIN|EVG_PROTOCOL_CMD_LONGIN_IDX_SOFTWARE_BUILD_DATE:
        break;

    default:
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                                                             "Invalid address");
        return asynError;
    }
    pdpvt->commandPacket.command = address;
    status = cmdWriteRead(pdpvt, pasynUser, 0, &nRead);
    if (status != asynSuccess) return status;
    if (nRead != 1) {
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                                                               "Bad read size");
        return asynError;
    }
    tBuf = pdpvt->replyPacket.args[0];
    len = strftime(data, maxchars, "%F %T", localtime(&tBuf));
    if (len == 0) {
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                                            "Target string buffer too small");
        return asynError;
    }
    *nTransferred = len;
    if (eomReason) {
        *eomReason = ASYN_EOM_END;
        if (len == maxchars) *eomReason |= ASYN_EOM_CNT;
    }
    return asynSuccess;
}
static asynOctet octetMethods = { NULL, octetRead };

/*
 * Find the interrupt callback handles for the sequencer status records
 */
static int
findSequencerStatusInterrupts(drvPvt *pdpvt, asynInt32Interrupt **interrupts)
{
    ELLLIST *pclientList;
    interruptNode *pnode;
    int foundMap = 0;
    pasynManager->interruptStart(pdpvt->asynInterfaces.int32InterruptPvt, &pclientList);
    pnode = (interruptNode *)ellFirst(pclientList);
    while (pnode) {
        asynInt32Interrupt *int32Interrupt = pnode->drvPvt;
        pnode = (interruptNode *)ellNext(&pnode->node);
        int a = int32Interrupt->addr;
        if ((a & (EVG_PROTOCOL_CMD_MASK_HI | EVG_PROTOCOL_CMD_MASK_LO)) ==
                                      (EVG_PROTOCOL_CMD_HI_LONGIN |
                                       EVG_PROTOCOL_CMD_LONGIN_LO_SEQ_STATUS)) {
            unsigned int idx = a & EVG_PROTOCOL_CMD_MASK_IDX;
            if (idx < EVG_PROTOCOL_EVG_COUNT) {
                interrupts[idx] = int32Interrupt;
                foundMap |= (1 << idx);
            }
        }
    }
    pasynManager->interruptEnd(pdpvt->asynInterfaces.int32InterruptPvt);
    return (foundMap == ((1 << EVG_PROTOCOL_EVG_COUNT) - 1));
}

/*
 * Sequencer status subscriber
 */
static void
subscriberThread(void *arg)
{
    drvPvt *pdpvt = (drvPvt *)arg;
    asynStatus status;
    size_t ntrans;
    epicsUInt32 pkNumber = 0;
    int subscriptionAttempt;
    epicsTimeStamp now, whenSubscribed, pkTime;
    asynInt32Interrupt *interrupts[EVG_PROTOCOL_EVG_COUNT];
    enum readState {rsUnknown, rsGood, rsBad} readState = rsUnknown;
    extern volatile int interruptAccept;

    while (!interruptAccept) epicsThreadSleep(1.0);
    if (!findSequencerStatusInterrupts(pdpvt, interrupts)) {
        errlogPrintf("==== FATAL ==== Can't find sequencer status records\n");
        return;
    }
    for (;;) {
        pdpvt->seqLink.isCommunicating = 0;
        subscriptionAttempt = 0;
        for (;;) {
            if (shutdown) return;
            status = pasynCommonSyncIO->connectDevice(
                                                pdpvt->seqLink.pasynUserCommon);
            if (status == asynSuccess)
                break;
            if (shutdown) return;
            asynPrint(pdpvt->seqLink.pasynUserCommon, ASYN_TRACE_ERROR,
                    "%s: Can't connect device: %s.  "
                    "This may be the result of an IOC shutdown, a network "
                    "problem or a problem with the network routing tables.\n",
                                  pdpvt->seqLink.portName,
                                  pdpvt->seqLink.pasynUserCommon->errorMessage);
            epicsThreadSleep(10.0);
        }
        for (;;) {
            struct evgStatusPacket pk;
            int eomReason;
            int i;
            if (shutdown) return;
            epicsTimeGetCurrent(&now);
            if (!pdpvt->seqLink.isCommunicating
             || (epicsTimeDiffInSeconds(&now, &whenSubscribed) >=
                                        SEQUENCER_STATUS_RESUBSCRIBE_SECONDS)) {
                epicsUInt32 magic = EVG_PROTOCOL_MAGIC;
                status = pasynOctetSyncIO->write(pdpvt->seqLink.pasynUserOctet,
                                                           (const char *)&magic,
                                                           sizeof(magic),
                                                           1.0,
                                                           &ntrans);
                if (status != asynSuccess) {
                    asynPrint(pdpvt->seqLink.pasynUserCommon, ASYN_TRACE_ERROR,
                                   "%s: Can't send subscription request: %s\n",
                                   pdpvt->seqLink.portName,
                                   pdpvt->seqLink.pasynUserOctet->errorMessage);
                }
                subscriptionAttempt++;
            }
            status = pasynOctetSyncIO->read(pdpvt->seqLink.pasynUserOctet,
               (char *)&pk, sizeof(pk),
               subscriptionAttempt ? 0.5 : SEQUENCER_STATUS_RESUBSCRIBE_SECONDS,
               &ntrans, &eomReason);
            if ((status == asynSuccess)
             && ((ntrans != sizeof(pk)) || (pk.magic != EVG_PROTOCOL_MAGIC))) {
                continue;
            }
            if ((status == asynTimeout) && (subscriptionAttempt < 2)) {
                continue;
            }
            if (status == asynSuccess) {
                pkTime.secPastEpoch = pk.posixSeconds-POSIX_TIME_AT_EPICS_EPOCH;
                pkTime.nsec = pk.ntpFraction / 4.294967296;
            }
            else {
                pkTime = now;
            }
            for (i = 0 ; i < EVG_PROTOCOL_EVG_COUNT ; i++) {
                asynInt32Interrupt *int32Interrupt = interrupts[i];
                asynUser *pasynUser = int32Interrupt->pasynUser;
                pasynUser->auxStatus = status;
                pasynUser->timestamp = pkTime;
                int32Interrupt->callback(int32Interrupt->userPvt,
                                              pasynUser, pk.sequencerStatus[i]);
            }
            if (status == asynSuccess) {
                int missed;
                if (readState == rsBad) {
                    asynPrint(pdpvt->seqLink.pasynUserCommon, ASYN_TRACE_ERROR,
                               "%s: Read succeeded\n", pdpvt->seqLink.portName);
                }
                readState = rsGood;
                if (subscriptionAttempt) {
                    subscriptionAttempt = 0;
                    whenSubscribed = now;
                }
                if (!pdpvt->seqLink.isCommunicating) {
                    pdpvt->seqLink.isCommunicating = 1;
                    pkNumber = pk.pkNumber - 1;
                    pdpvt->seqConnectCount++;
                }
                missed = (pk.pkNumber - pkNumber) - 1;
                if (missed > 0) {
                    pdpvt->seqMissedCount += missed;
                }
                pdpvt->seqReceivedCount++;
                pkNumber = pk.pkNumber;
            }
            else {
                if (readState != rsBad) {
                    asynPrint(pdpvt->seqLink.pasynUserCommon, ASYN_TRACE_ERROR,
                                   "%s: Read failed: %s\n",
                                   pdpvt->seqLink.portName,
                                   pdpvt->seqLink.pasynUserOctet->errorMessage);
                    readState = rsBad;
                }
                break;
            }
        }
        if (pasynCommonSyncIO->disconnectDevice(pdpvt->seqLink.pasynUserCommon)
                                                               != asynSuccess) {
            errlogPrintf("==== FATAL ==== %s can't disconnect: %s\n",
                                pdpvt->cmdLink.portName,
                                pdpvt->cmdLink.pasynUserCommon->errorMessage);
            return;
        }
    }
}

/*
 * Create a new lower port and set up a link to it
 */
static asynStatus
setLink(portLink *link, drvPvt *pdpvt, const char *ext, const char *hostInfo,
                                                          unsigned int priority)
{
    asynStatus status;

    link->portName = callocMustSucceed(1, strlen(pdpvt->portName)+strlen(ext)+1, "bcm");
    sprintf(link->portName, "%s%s", pdpvt->portName, ext);
    link->hostInfo = epicsStrDup(hostInfo);
    /* No autoconnect, No process EOS */
    drvAsynIPPortConfigure(link->portName, link->hostInfo, priority, 1, 1);
    status = pasynCommonSyncIO->connect(link->portName, -1,
                                       &link->pasynUserCommon, NULL);
    if (status != asynSuccess) {
        errlogPrintf("Can't set asynCommonSyncIO for port \"%s\".\n", link->portName);
        return asynError;
    }
    status = pasynOctetSyncIO->connect(link->portName, -1,
                                       &link->pasynUserOctet, NULL);
    if (status != asynSuccess) {
        errlogPrintf("Can't set asynOctetSyncIO for port \"%s\".\n", link->portName);
        return asynError;
    }
    return asynSuccess;
}

static void
evgConfigure(const char *portName, const char *hName, unsigned int priority)
{
    drvPvt *pdpvt;
    asynStandardInterfaces *pInterfaces;
    asynStatus status;
    char *host;
    int hNameLen;
    unsigned int threadPriority;
    epicsThreadId tid;
    static int firstTime = 1;

    /*
     * Set up local storage
     */
    if (firstTime) {
        epicsAtExit(atExitHandler, NULL);
        firstTime = 0;
    }
    pdpvt = (drvPvt *)callocMustSucceed(1, sizeof(drvPvt), portName);
    pdpvt->portName = epicsStrDup(portName);
    if (priority == 0) priority = epicsThreadPriorityMedium;
    pdpvt->pasynUser= pasynManager->createAsynUser(NULL, NULL);

    /*
     * Set up full information for connection to FPGA
     */
    if (strchr(hName, ':') != NULL) {
        errlogPrintf("Host info must not specify port.\n");
        return;
    }
    hNameLen = strlen(hName);
    if (hName[hNameLen - 1] == '*') {
        hNameLen--;
        errlogPrintf("Warning -- Broadcast designator ignored.\n");
    }
    host = (char *)callocMustSucceed(1, hNameLen + 30, "evgConf");

    /*
     * Create the ports that we'll use to communicate with the FPGA
     */
    sprintf(host, "%.*s:%d UDP", hNameLen, hName, EVG_PROTOCOL_UDP_EPICS_PORT);
    status = setLink(&pdpvt->cmdLink, pdpvt, "_CMD", host, priority);
    if (status != asynSuccess)
        return;
    epicsThreadLowestPriorityLevelAbove (priority, &priority);
    sprintf(host, "%.*s:%d UDP", hNameLen, hName, EVG_PROTOCOL_UDP_STATUS_PORT);
    status = setLink(&pdpvt->seqLink, pdpvt, "_SEQ", host, priority);
    if (status != asynSuccess)
        return;

    /*
     * Create our port
     */
    status = pasynManager->registerPort(pdpvt->portName,
                                        ASYN_CANBLOCK|ASYN_MULTIDEVICE,
                                        1, priority, 0);
    if(status != asynSuccess) {
        errlogPrintf("registerPort failed\n");
        return;
    }

    /*
     * Register ASYN interfaes
     */
    pInterfaces = &pdpvt->asynInterfaces;
    pInterfaces->common.pinterface        = &commonMethods;
    pInterfaces->int32.pinterface         = &int32Methods;
    pInterfaces->uInt32Digital.pinterface = &uint32DigitalMethods;
    pInterfaces->int32Array.pinterface    = &int32ArrayMethods;
    pInterfaces->octet.pinterface         = &octetMethods;
    pInterfaces->int32CanInterrupt        = 1;
    status = pasynStandardInterfacesBase->initialize(pdpvt->portName,
                                                     pInterfaces,
                                                     pdpvt->pasynUser, pdpvt);
    if (status != asynSuccess) {
        errlogPrintf("Can't register interfaces: %s.\n",
                                                pdpvt->pasynUser->errorMessage);
        return;
    }

    /*
     * Start the sequencer status subscriber thread.
     */
    epicsThreadLowestPriorityLevelAbove(priority, &threadPriority);
    pdpvt->threadName = callocMustSucceed(1,strlen(portName)+12,"evgConf");
    sprintf(pdpvt->threadName, "%s_SUBCRIBER", portName);
    tid = epicsThreadCreate(pdpvt->threadName,
                            threadPriority,
                            epicsThreadGetStackSize(epicsThreadStackMedium),
                            subscriberThread,
                            pdpvt);
    if (!tid) {
        printf("Can't set up %s subscriber thread!\n", pdpvt->threadName);
        return;
    }
}

/*
 * IOC shell command registration
 */
static const iocshArg evgConfigureArg0 = { "port",iocshArgString};
static const iocshArg evgConfigureArg1 = { "host",iocshArgString};
static const iocshArg evgConfigureArg2 = { "priority",iocshArgInt};
static const iocshArg *evgConfigureArgs[] = {
                    &evgConfigureArg0,
                    &evgConfigureArg1,
                    &evgConfigureArg2 };
static const iocshFuncDef evgConfigureFuncDef =
      {"eventGeneratorConfigure", 3, evgConfigureArgs};
static void evgConfigureCallFunc(const iocshArgBuf *args)
{
    evgConfigure(args[0].sval, args[1].sval, args[2].ival);
}

static void
eventGenerator_RegisterCommands(void)
{
    iocshRegister(&evgConfigureFuncDef,evgConfigureCallFunc);
}
epicsExportRegistrar(eventGenerator_RegisterCommands);
