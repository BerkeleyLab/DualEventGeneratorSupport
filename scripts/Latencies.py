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
args = parser.parse_args()

class EVG:
    def __init__(self, prefix, evg):
        self.localLatency = -1.0
        self.latency = epics.PV(args.prefix + 'E%d:latency' % evg,form='time')
        self.loopback = epics.PV(args.prefix + 'E%d:loopback' % evg,form='time')

    def getAbsoluteLatencyForChannel(self, chan):
        if ((chan < 0) or (chan > 36)):
            raise ValueError("Channel out of range")
        self.loopback.put(chan, wait=True)
        self.loopback.get_timevars()
        while True:
            self.latency.get_timevars()
            if self.latency.timestamp >= self.loopback.timestamp: break
            time.sleep(0.001)
        return self.latency.get()

    def getRelativeLatencyForChannel(self, chan):
        # Lazy initializaiton of local latency measurement
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
