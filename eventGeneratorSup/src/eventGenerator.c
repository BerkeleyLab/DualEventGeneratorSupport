/*
 * ALSU event generator device support
 */
#include <string.h>
#include <epicsStdio.h>
#include <epicsString.h>
#include <epicsEvent.h>
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
#include <asynStandardInterfaces.h>
#include "evgProtocol.h"

/*
 * Time conversion -- FIXME: Configuration parameter(s)
 */
#define NOMINAL_RF_FREQUENCY 499.642e6
#define NS_PER_TICK (1.0e9 / ((NOMINAL_RF_FREQUENCY / 4)))

/*
 * Local ASYN subaddress
 * Augments values in evgProtocol.h
 */
#define A_HI_IOC                0xF000
# define A_IOC_LO_STATISTICS                0x000

/*
 * Number of times to retry a command
 */
#define COMMAND_RETRY_LIMIT 4

/*
 * Links to lower port
 */
typedef struct portLink {
    char             *portName;
    char             *hostInfo;
    asynUser         *pasynUserCommon;
    asynUser         *pasynUserOctet;
    asynInterface    *poctet;
    int               isCommunicating;
} portLink;

/*
 * Driver private storage
 */
typedef struct drvPvt {
    /*
     * Link to lower-level port driver
     */
    portLink        cmdLink;

    /*
     * Asyn interfaces we provide
     */
    char           *portName;
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
    int             timeDifferenceWarned;
    int             timeDifferenceWarnCount;
} drvPvt;

/*
 * asynCommon methods
 */
static void
report(void *pvt, FILE *fp, int details)
{
    drvPvt *pdpvt = (drvPvt *)pvt;

    fprintf(fp, " %s %scommunicating\n", pdpvt->cmdLink.portName,
                                pdpvt->cmdLink.isCommunicating ? "" : "NOT ");
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
    size_t cmdSize = EVG_PROTOCOL_ARG_COUNT_TO_SIZE(cmdArgCount);
    size_t nTrans;
    int eom;
    asynOctet *pasynOctet = pdpvt->cmdLink.poctet->pinterface;
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
        pasynManager->lockPort(pdpvt->cmdLink.pasynUserOctet);
        if (omitSend) {
            status = asynSuccess;
            omitSend = 0;
        }
        else {
            status = pasynOctet->write(pdpvt->cmdLink.poctet->drvPvt,
                                                  pdpvt->cmdLink.pasynUserOctet,
                                                  (char *)&pdpvt->commandPacket,
                                                  cmdSize, &nTrans);
        }
        if (status == asynSuccess) {
            status = pasynOctet->read(pdpvt->cmdLink.poctet->drvPvt,
                                      pdpvt->cmdLink.pasynUserOctet,
                                      (char *)&pdpvt->replyPacket,
                                      sizeof pdpvt->replyPacket, &nTrans, &eom);
        }
        pasynManager->unlockPort(pdpvt->cmdLink.pasynUserOctet);
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
 * Get time from data or system
 */
static void
setTimestamp(drvPvt *pdpvt, int isValid,
              epicsUInt32 seconds, epicsUInt32 ticks, epicsTimeStamp *ts)
{
    double x;
    epicsTimeStamp iocTime;

    epicsTimeGetCurrent(&iocTime);
    if (isValid && (seconds != 0) && ((x = ticks * NS_PER_TICK) < 5.5e9)) {
        int diff;
        if (seconds > POSIX_TIME_AT_EPICS_EPOCH)
            ts->secPastEpoch = seconds - POSIX_TIME_AT_EPICS_EPOCH;
        else
            ts->secPastEpoch = seconds;
        if (x < 1e9) {
            ts->nsec = x;
        }
        else {
            uint64_t n = x;
            ts->secPastEpoch += n / 1000000000;
            ts->nsec = n % 1000000000;
        }
        diff = ts->secPastEpoch - iocTime.secPastEpoch;
        if (abs(diff) > 5) {
            if (!pdpvt->timeDifferenceWarned) {
                if (pdpvt->timeDifferenceWarnCount < 100) {
                    asynPrint(pdpvt->cmdLink.pasynUserOctet, ASYN_TRACE_ERROR,
                               "%s: FPGA(%lu) - IOC(%lu) seconds: %d.\n",
                                            pdpvt->portName,
                                            (unsigned long)ts->secPastEpoch,
                                            (unsigned long)iocTime.secPastEpoch,
                                            diff);
                    pdpvt->timeDifferenceWarned = 1;
                    pdpvt->timeDifferenceWarnCount++;
                }
            }
        }
        else {
            if (pdpvt->timeDifferenceWarned) {
                asynPrint(pdpvt->cmdLink.pasynUserOctet, ASYN_TRACE_ERROR,
                          "%s: FPGA/IOC time stamps agree.\n", pdpvt->portName);
                pdpvt->timeDifferenceWarned = 0;
            }
        }
    }
    else {
        if (isValid && !pdpvt->timeDifferenceWarned) {
            asynPrint(pdpvt->cmdLink.pasynUserOctet, ASYN_TRACE_ERROR,
                         "%s: FPGA time(%lu:%lu) bad -- using IOC time.\n",
                                                         pdpvt->portName,
                                                         (unsigned long)seconds,
                                                         (unsigned long)ticks);
            pdpvt->timeDifferenceWarned = 1;
        }
        *ts = iocTime;
    }
}

/*
 * Push monitor data into records
 */
static void
processMonitorPacket(drvPvt *pdpvt, asynStatus status, int replyArgCount)
{
    ELLLIST *pclientList;
    interruptNode *pnode;
    epicsTimeStamp when;

    if ((status == asynSuccess) && (replyArgCount < 2)) {
        status = asynError;
    }
    setTimestamp(pdpvt, (status == asynSuccess),
                 pdpvt->replyPacket.args[0], pdpvt->replyPacket.args[1], &when);
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
            int32Interrupt->pasynUser->timestamp = when;
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
            *value = pdpvt->replyPacket.args[0] & 0xFFFF;
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
            if (idx <= COMMAND_RETRY_LIMIT)
                *value = pdpvt->commandCount[idx];
            else
                *value = pdpvt->commandFailedCount;
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
uint32DigitalRead(void *pvt, asynUser *pasynUser, epicsUInt32 *value,
                                                               epicsUInt32 mask)
{
    drvPvt *pdpvt = (drvPvt *)pvt;
    int address;
    asynStatus status;
    int nRead;

    if ((status = pasynManager->getAddr(pasynUser, &address)) != asynSuccess)
        return status;
    pdpvt->commandPacket.command = address;
    status = cmdWriteRead(pdpvt, pasynUser, 0, &nRead);
    if ((status == asynSuccess) && (nRead != 1)) {
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                                                               "Bad read size");
        return asynError;
    }
    *value = pdpvt->replyPacket.args[0] & mask;
    return status;
}
static asynUInt32Digital uint32DigitalMethods = { NULL, uint32DigitalRead };

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
        if ((n < 2)
         || (n % 2)
         || (value[n-1] != EVG_PROTOCOL_WAVEFORM_END_OF_TABLE_EVENT_CODE)) {
            epicsSnprintf(pasynUser->errorMessage,
                                pasynUser->errorMessageSize, "No end-of-table");
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
             || (nSend == EVG_PROTOCOL_ARG_CAPACITY)
             || ((nSend == (EVG_PROTOCOL_ARG_CAPACITY-1))
              && (*value>=EVG_PROTOCOL_WAVEFORM_SINGLE_WORD_DELAY_LIMIT))) {
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
 * Create a new lower port and set up a link to it
 */
static asynStatus
setLink(portLink *link, drvPvt *pdpvt, const char *ext, const char *hostInfo, int priority)
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
    link->pasynUserOctet= pasynManager->createAsynUser(NULL, NULL);
    status = pasynManager->connectDevice(link->pasynUserOctet, link->portName, -1);
    if (status != asynSuccess) {
        errlogPrintf("Can't find asyn port \"%s\".\n", link->portName);
        return asynError;
    }
    link->poctet = pasynManager->findInterface(link->pasynUserOctet, asynOctetType, 0);
    if (link->poctet == NULL) {
        errlogPrintf("Can't find octet interface for \"%s\".\n", link->portName);
        return asynError;
    }
    link->pasynUserOctet->timeout = 5.0;
    return asynSuccess;
}

static void
evgConfigure(const char *portName, const char *hostName, int priority)
{
    drvPvt *pdpvt;
    asynStandardInterfaces *pInterfaces;
    asynStatus status;
    char *host;
    int hostNameLen;

    /*
     * Set up local storage
     */
    pdpvt = (drvPvt *)callocMustSucceed(1, sizeof(drvPvt), portName);
    pdpvt->portName = epicsStrDup(portName);
    if (priority == 0) priority = epicsThreadPriorityMedium;
    pdpvt->pasynUser= pasynManager->createAsynUser(NULL, NULL);

    /*
     * Set up full information for connection to FPGA
     */
    if (strchr(hostName, ':') != NULL) {
        errlogPrintf("Host info must not specify port.\n");
        return;
    }
    hostNameLen = strlen(hostName);
    if (hostName[hostNameLen - 1] == '*') {
        hostNameLen--;
        errlogPrintf("Warning -- Broadcast designator ignored.\n");
    }
    host = (char *)callocMustSucceed(1, hostNameLen + 30, "bcmConf");
    sprintf(host, "%.*s:%d UDP", hostNameLen, hostName, EVG_PROTOCOL_UDP_PORT);

    /*
     * Create the port that we'll use to communicate with the FPGA
     */
    status = setLink(&pdpvt->cmdLink, pdpvt, "_CMD", host, priority);
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
