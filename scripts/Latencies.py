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
args = parser.parse_args()

class EVG:
    def __init__(self, prefix, evg):
        self.latency = (epics.PV(args.prefix + 'E%d:latency' % evg))
        self.loopback = (epics.PV(args.prefix + 'E%d:loopback' % evg))
        self.offset = self.getLatencyForChannel(36)

    def getLatencyForChannel(self, chan):
        if ((chan < 0) or (chan > 36)):
            raise ValueError("Channel out of range")
        self.loopback.put(chan, wait=True)
        time.sleep(0.02)
        return self.latency.get()

evgs = []
for e in (1, 2):
    evgs.append(EVG(args.prefix, e))
for channel in range(1, 37):
    print('%2d:' % (channel), end='')
    for evg in evgs:
        print('%7.1f' % (evg.getLatencyForChannel(channel)), end='')
    print('')
