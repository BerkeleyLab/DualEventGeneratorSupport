#!/usr/bin/env python

#
# Check ALS-U refClk/txClk alignment
#

from __future__ import print_function
import argparse
import epics
import numpy
import sys
import time

parser = argparse.ArgumentParser(description='Demonstrate sequencer operation.', formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument('-c', '--cycles', type=int, default=10, help='Number of acquisition cycles')
parser.add_argument('-p', '--prefix', default='EVG:', help='Record name prefix')
args = parser.parse_args()

reboot = epics.PV(args.prefix + 'FPGA:reboot')
find = epics.PV(args.prefix + 'findCoinc')
rbk = epics.PV(args.prefix + 'sysmonTrig_.PROC')
coincTable = []
for e in ('1', '2'):
    for c in ('ref', 'tx'):
        pv = epics.PV(args.prefix + 'E' + e + ':' + c + 'Coinc')
        coincTable.append(pv)

histogramCount = len(coincTable)
histogramBinCount = 610
histograms = numpy.zeros((histogramCount, histogramBinCount))

while args.cycles > 0:
    args.cycles -= 1
    reboot.put(0,wait='true')
    time.sleep(0.1)
    find.put(0,wait='true')
    time.sleep(0.2)
    rbk.put(0,wait='true')
    time.sleep(0.2)
    i = 0
    for p in coincTable:
        b = int(p.get())
        if b < 0: b = histogramBinCount - 1
        if b < histogramBinCount: histograms[i][b] += 1
        i += 1


i = 0
for p in coincTable:
    for j in range(0,histogramBinCount):
        if histograms[i][j] != 0:
            print('%d:%d' % (j, histograms[i][j]), end = ' ')
    print()
    i += 1
