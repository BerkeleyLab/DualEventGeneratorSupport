#!/usr/bin/env python

#
# Measure latencies on all channels
# FIXME: Populate fanout topology and actually *use* it.
#

from __future__ import print_function
import argparse
import epics
import sys
import time

# Fanout module topology
# Tuple of dictionaries, one per event generator frequency domain
#   Key is event generator channel
#   Value is record name prefix for corresponding fanout
# FIXME: What about multiple fanout levels?
fanoutModules = ( { } ,
                  { } )
parser = argparse.ArgumentParser(description='Measure latencies on all channels.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument('-p', '--prefix', default='EVG:', help='Record name prefix')
parser.add_argument('-s', '--short', action='store_true', help='Show only non-zero latencies.')
parser.add_argument('-v', '--verbose', action='store_true', help='Enable some additional diagnostic messages.')
args = parser.parse_args()

class EVG:
    def __init__(self, prefix, evg):
        self.initialLoopback = -1
        self.latency = epics.PV(args.prefix + 'E%d:latency'%(evg),form='time',auto_monitor=True)
        self.loopback = epics.PV(args.prefix + 'E%d:loopback'%(evg),form='time',auto_monitor=True)
        self.loopbackPROC = epics.PV(args.prefix + 'E%d:loopback.PROC'%(evg))
        self.initialLoopback = self.loopback.get();

    def getLatencyForChannel(self, chan):
        if ((chan < 0) or (chan > 36)):
            raise ValueError("Channel out of range")
        self.loopback.put(chan, wait=True)
        passCount = 0
        while self.latency.timestamp < self.loopback.timestamp:
            passCount += 1
            if passCount > 1000:
                print("Timed out waiting for updated data.", file=sys.stderr)
            time.sleep(0.01)
            self.latency.get_timevars()
        passCount = 0
        while self.latency.severity == 3:  # SEVR=INVALID
            self.loopbackPROC.put(1)
            passCount += 1
            if passCount > 1000:
                print("Timed out waiting for valid data.", file=sys.stderr)
            time.sleep(0.01)
        if args.verbose and passCount > 0:
            print("Readout pass %d" % (passCount), file=sys.stderr)
        l = self.latency.get()
        return l

    def restore(self):
        if self.initialLoopback >= 0:
            self.loopback.put(self.initialLoopback, wait=True)
        self.initialLoopback = -1

def show(evg, channel):
    global args
    l = evg.getLatencyForChannel(channel)
    if l != 0 or not args.short:
        print('%2d: %6.1f' % (channel, l))

for e in (1, 2):
    evg = EVG(args.prefix, e)
    fanoutDict = fanoutModules[e-1]
    for channel in range(1, 37):
        if channel in fanoutDict:
            # FIXME Here's where the EVF should be scanned
            print(fanoutDict[channel])
        else:
            show(evg, channel)
    evg.restore()
