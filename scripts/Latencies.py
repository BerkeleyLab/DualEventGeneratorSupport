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
        self.internalLatency = 0.0
        self.latency = epics.PV(args.prefix + 'E%d:latency' % evg,form='time')
        self.loopback = epics.PV(args.prefix + 'E%d:loopback' % evg,form='time')

    def setInternalLatency(self, newInternalLatency=None):
        if newInternalLatency == None:
            self.internalLatency = self.getLatencyForChannel(36)
        else:
            self.internalLatency = newInternalLatency

    def getLatencyForChannel(self, chan):
        if ((chan < 0) or (chan > 36)):
            raise ValueError("Channel out of range")
        self.loopback.put(chan, wait=True)
        self.loopback.get_timevars()
        while True:
            self.latency.get_timevars()
            if self.latency.timestamp >= self.loopback.timestamp: break
            time.sleep(0.001)
        l = self.latency.get()
        if chan == 36:
            return l
        else:
            return l - self.internalLatency

evgs = []
for e in (1, 2):
    evg = EVG(args.prefix, e)
    evgs.append(evg)
    if args.external: evg.setInternalLatency()
for channel in range(1, 37):
    print('%2d:' % (channel), end='')
    for evg in evgs:
        print('%7.1f' % (evg.getLatencyForChannel(channel)), end='')
    print('')
