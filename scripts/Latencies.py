#!/usr/bin/env python

#
# Measure latencies on all channels
#

from __future__ import print_function
import argparse
import epics
import sys
import time

parser = argparse.ArgumentParser(description='Measure latencies on all channels.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument('-p', '--prefix', default='EVG:', help='Record name prefix')
parser.add_argument('-e', '--external', action='store_true', help='Measure external latency by subtracting local FPGA to crosspoint switch latency')
parser.add_argument('-v', '--verbose', action='store_true', help='Enable some additional diagnostic messages.')
args = parser.parse_args()

class EVG:
    def __init__(self, prefix, evg):
        self.localLatency = -1.0
        self.latency = epics.PV(args.prefix + 'E%d:latency'%(evg),form='time',auto_monitor=True)
        self.loopback = epics.PV(args.prefix + 'E%d:loopback'%(evg),form='time',auto_monitor=True)
        self.loopbackPROC = epics.PV(args.prefix + 'E%d:loopback.PROC'%(evg))

    def getAbsoluteLatencyForChannel(self, chan):
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
        while self.latency.severity != 0:
            self.loopbackPROC.put(1)
            passCount += 1
            if passCount > 1000:
                print("Timed out waiting for valid data.", file=sys.stderr)
            time.sleep(0.01)
        if args.verbose and passCount > 0:
            print("Readout pass %d" % (passCount), file=sys.stderr)
        l = self.latency.get()
        return l

    def getRelativeLatencyForChannel(self, chan):
        # Lazy initialization of local latency measurement
        if self.localLatency < 0:
            self.localLatency = self.getAbsoluteLatencyForChannel(36)
        l = self.getAbsoluteLatencyForChannel(chan)
        # Local loopback measurement is always absolute
        if chan < 36: l -= self.localLatency
        return l

evgs = []
for e in (1, 2):
    evgs.append(EVG(args.prefix, e))
for channel in range(1, 37):
    print('%2d:' % (channel), end='')
    for evg in evgs:
        if args.external:
            l = evg.getRelativeLatencyForChannel(channel)
        else:
            l = evg.getAbsoluteLatencyForChannel(channel)
        print('%7.1f' % (l), end='')
    print('')
