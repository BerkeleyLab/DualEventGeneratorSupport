#!/usr/bin/env python

#
# Act as ALS timing system client
#

from __future__ import print_function
import argparse
import epics
import sys
import time

parser = argparse.ArgumentParser(description='Demonstrate timing sequencer operation.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument('-e', '--evg', default='testEVG:', help='Event geneerator Record name prefix')
parser.add_argument('-t', '--test', default='test', help='Timing system test prefix')
parser.add_argument('-v', '--verbose', action='store_true', help='Show outgoing requests')
args = parser.parse_args()

def pv(name):
    pv = epics.PV(name)
    pv.get()
    if not pv.connect():
        print('Unable to connect to "%s"' % (name))
        sys.exit(1)
    return pv

# Synchronize with booster cycle
seqStatus = pv(args.evg + 'E1:seqStatus')
seqStatusBusy = 0x10

# Show event generator updates
sequence = pv(args.evg + 'E1:SEQ1')
sequenceNORD = pv(args.evg + 'E1:SEQ1.NORD')

# 'Temporary' field delays
TimInjFieldSyncDelaySP = pv(args.test + 'TimInjFieldSyncDelaySP')
TimExtrFieldSyncDelaySP = pv(args.test + 'TimExtrFieldSyncDelaySP')

# Injection request
TARGET_BUCKET = 0
GUN_BUNCHES   = 4
INJ_MODE      = 2
GUN_INHIBIT   = 3
RESERVED1     = 4
RESERVED2     = 5
SEQUENCE      = 6
request = [1, 4, 70, 0, 0, 0, 1]
requestPV = pv(args.test + 'TimInjReq')
bucketIndex = 0

# Show the sequence requests
def sequenceCallback(pvname=None, value=None, **kws):
    global sequenceNORD
    print(sequenceNORD, sequence)

checks = 0;
while (seqStatus.get() == None):
    time.sleep(0.5)
    checks += 1
    if checks > 5:
        print('Unable to connect')
        sys.exit(1)
sequence.add_callback(sequenceCallback)

while True:
    while (seqStatus.get() & seqStatusBusy) == 0:
        time.sleep(0.05)
    while (seqStatus.get() & seqStatusBusy) != 0:
        time.sleep(0.05)
    bucketIndex = (bucketIndex + 1) % 328
    request[TARGET_BUCKET] = bucketIndex + 1
    request[SEQUENCE] += 1
    requestPV.put(request)
    TimInjFieldSyncDelaySP.put(100000)
    TimExtrFieldSyncDelaySP.put(1000000)
    if (args.verbose): print(request)
