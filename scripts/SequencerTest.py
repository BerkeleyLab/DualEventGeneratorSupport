#!/usr/bin/env python

#
# ALS-U dual event generator example
#

from __future__ import print_function
import argparse
import epics
import sys
import time

oldSequenceNumber = None
seqDone = False
seqCount = [0, 0]
def seqStatusCallback(pvname=None, value=None, **kws):
    global oldSequenceNumber, seqDone
    isActive = value & 0x8
    sequenceNumber = (value >> 8) & 0xFF
    diff = (sequenceNumber - oldSequenceNumber) & 0xFF
    if diff > 1:
        print('Missed %d' % (diff - 1), file=sys.stderr)
        sys.exit(2)
    if (diff == 1) and (isActive == 0):
        global seqCount
        seqCount[(value & 0x4) >> 2] += 1
        oldSequenceNumber = sequenceNumber
        seqDone = True

def awaitSequenceCompletion():
    global seqDone
    then = time.time()
    while not seqDone:
        if (time.time() - then) > 3.5:
            print('Timed out waiting for sequence completion.', file=sys.stderr)
            sys.exit(3)
        time.sleep(0.03)
    seqDone = False

def parseSequence(delayEventPairs):
    list = delayEventPairs.split(',')
    seq = []
    isEvent = False
    for l in list:
        try:
            v = int(l)
        except:
            print("Bad sequence", file=sys.stderr)
            sys.exit(1)
        if (isEvent):
            if ((v <= 0) or (v >= 256)):
                print("Bad event (%d)" % (v), file=sys.stderr)
                sys.exit(1)
        else:
            if ((v < 0) or (v > 2e9)):
                print("Bad delay (%d)" % (v), file=sys.stderr)
                sys.exit(1)
        seq.append(v)
        isEvent = not isEvent
    if (isEvent):
        print("Missing event", file=sys.stderr)
        sys.exit(1)
    if (v != 127):
        print("Warning -- End-of-sequence event missing, will append one for you", file=sys.stderr)
        seq.append(0)
        seq.append(127)
    return seq


parser = argparse.ArgumentParser(description='Demonstrate sequencer operation.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument('-c', '--cycles', type=int, default=0, help='Number of "sequence 1" cycles for EVG 1, number of "sequence 0" cycles for EVG 2')
parser.add_argument('-e', '--evg', type=int, default=1, choices=(1,2), help='Event generator to use')
parser.add_argument('-p', '--prefix', default='EVG:', help='Record name prefix')
parser.add_argument('-0', '--seq0', default='0,10,1816948,12,3,18,0,20,1,24,0,26,0,28,57764862,39,649328,38,661,50,2,56,2499991,70,0,127', help='Sequence 0')
parser.add_argument('-1', '--seq1', default='0,10,1816948,12,3,18,0,20,1,24,0,26,0,28,57764862,39,649328,38,661,50,2,56,2499950,68,40,70,0,127', help='Sequence 1')
args = parser.parse_args()

pattern0 = parseSequence(args.seq0)
pattern1 = parseSequence(args.seq1)

seqStatus = epics.PV(args.prefix + 'E%d:seqStatus' % (args.evg))
seq0 = epics.PV(args.prefix + "E%d:SEQ0" % (args.evg))
seq1 = epics.PV(args.prefix + "E%d:SEQ1" % (args.evg))
seq0enable = epics.PV(args.prefix + "E%d:SEQ0:enable" % (args.evg))
seq1enable = epics.PV(args.prefix + "E%d:SEQ1:enable" % (args.evg))
if args.evg == 1:
    seq1enable.put(0, wait=True)
    while (seqStatus.get() & 0x8): time.sleep(0.1)
    seq1.put(pattern1, wait=True)
else:
    swapoutTrigger = epics.PV(args.prefix + "swapoutTrigger")
    seq0enable.put(0, wait=True)
    seq1enable.put(0, wait=True)
    while (seqStatus.get() & 0x8): time.sleep(0.1)
    seq0.put(pattern0, wait=True)
    seq1.put(pattern1, wait=True)
    seq0enable.put(1, wait=True)
oldSequenceNumber = (seqStatus.get() >> 8) & 0xFF
seqStatus.add_callback(seqStatusCallback)

next = 0.0;
if args.cycles > 0:
    while (seqStatus.get() & 0x8): time.sleep(0.1)
    seq0Count = seqCount[0]
    while args.cycles > 0:
        if args.evg == 1:
            seq1enable.put(1)
            awaitSequenceCompletion()
            if seqCount[0] != seq0Count:
                print("Missed sequence!", file=sys.stderr)
                seq0Count = seqCount[0]
        else:
            if (args.cycles % 10) == 0: seq1enable.put(1)
            pause = next - time.time()
            if pause > 0: time.sleep(pause)
            next = time.time() + 1.4
            swapoutTrigger.put(1)
            awaitSequenceCompletion()
        args.cycles -= 1
sys.exit(0)
