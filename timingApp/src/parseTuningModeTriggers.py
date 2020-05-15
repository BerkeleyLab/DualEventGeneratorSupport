#!/usr/bin/env python
#
# parseTuningModeTriggers.py: Takes the exported Google Spreadsheet tab maintained by
# G. Portmann/J. Weber/B. Gunion as input, and outputs a c file that defines all the 
# event codes and timestamps for the tuning modes.
# Expects as input the exported spreadsheet in TuningModeTriggers.csv.
# Writes output to tuningModes.c.
#
# @date April 2017
# @author Bob Gunion

from __future__ import print_function
import sys
import re

fin = open('TuningModeTriggers.csv')
if not fin:
    print('Cannot find TuningModeTriggers.csv!', file=sys.stderr)
    sys.exit(-2)

fout = open('tuningModes.c', 'w')
if not fout:
    print('Cannot open tuningModes.c for writing!', file=sys.stderr)
    sys.exit(-2)

# Signal,default?,Linac?,Booster?,BTS?,SRInjection?,SRInjPrep?,evtcode,tsoffset,functime,offsetticks
re_trigger = re.compile(r'([^,]+),(x?),(x?),(x?),(x?),(x?),(x?),(\d+),(\d+),([^,]+),(-?\d*).*')

functimeprefixes = [
    'Start',
    'InjFieldSync + GunBunchDelay + #Bunches',
    'InjFieldSync + GunBunchDelay',
    'InjFieldSync',
    'ExtrFieldSync + TargetBucket',
    'ExtrFieldSync',
    'End'
]
functimes = [
    '0',
    'syncDelays[INJ_SYNCDELAY_INDEX] + gunBunchesDelay + nBunches',
    'syncDelays[INJ_SYNCDELAY_INDEX] + gunBunchesDelay',
    'syncDelays[INJ_SYNCDELAY_INDEX]',
    'syncDelays[EXTR_SYNCDELAY_INDEX] + getTargetBucketDelay(targetBucket)',
    'syncDelays[EXTR_SYNCDELAY_INDEX]',
    'DELAY_END'
]

class Trigger:
    def __init__(self, m):
        self.name = m.group(1)
        self.evtcode = m.group(8)
        self.tsoffset = m.group(9)
        self.functime = m.group(10)
        self.offsetticks = m.group(11)
        self.modes = []
        for i in range(2, 8):
            self.modes.append(m.group(i) == 'x')

    def getTimestamp(self):
        index = 0
        found = False
        for prefix in functimeprefixes:
            if self.functime.startswith(prefix):
                found = True
                break
            index += 1
        if not found:
            print('ERROR: Invalid functional time string:',self.functime, file=sys.stderr)
            sys.exit(-2)
        str = functimes[index]+' + '+self.tsoffset
        if len(self.offsetticks) > 0:
            str += ' + '+self.offsetticks
        return str

def sortEvtCode(t):
    return t.evtcode

triggers = []

for line in fin:
    m = re_trigger.match(line)
    if not m:
        continue
    triggers.append(Trigger(m))
fin.close()

triggers = sorted(triggers, key = lambda t: int(t.evtcode))

fout.write('''/**
 * tuningModes.c
 *
 * THIS FILE IS GENERATED - ANY CHANGES WILL BE LOST WHEN THIS IOC IS BUILT
 *
 * Defines functions whose return values depend on the Timing PV Names Google Spreadsheet.
 * Generated from TuningModeTriggers.csv
 */

#include "tuningModes.h"
#include "timingSequenceDefs.h"
#include <stdio.h>

/*
 * Convert target bucket to delay
 */
static int
getTargetBucketDelay(int targetBucket)
{
	return (125 * ((21 * targetBucket) % 328)) / 4;
}

int getTimestamp(unsigned char evtcode, int *syncDelays, int nBunches, int gunBunchesDelay, int targetBucket) {
''')

#print 'Found %d trigger defs' % len(triggers)
priorEvtCodes = []
first = True
for t in triggers:
    #print t.name, t.modes[0], t.modes[1], t.modes[2], t.modes[3], t.modes[4], t.modes[5], t.evtcode, t.tsoffset, t.functime, t.offsetticks
    #print t.getTimestamp()
    if t.evtcode in priorEvtCodes:
        continue
    priorEvtCodes.append(t.evtcode)
    if first:
        first = False
        fout.write('    switch (evtcode) {\n')
    fout.write('    case '+t.evtcode+': return '+t.getTimestamp()+';\n')

fout.write('''    case 127: return DELAY_END + 1;
    default:
        printf("Invalid event code %d passed to getTimestamp; returning -1\\n", evtcode);
        return -1;
    }
}''')

fout.write('''

unsigned char modeEvtCodes[NUM_MODES][MAX_SEQUENCE_LENGTH] = {
''')

modes = [
    0,  # default
    10, # Linac/LTB
    20, # Booster
    30, # BTS
    40, # SR Injection
    41  # SR Inj Prep
]
first = True
for mode in range(0, 6):
    if first:
        first = False
        fout.write('    {\n')
    else:
        fout.write('    },\n    {\n')
    priorEvtCodes = []
    tfirst = True
    for t in triggers:
        if not t.modes[mode]:
            continue
        if t.evtcode in priorEvtCodes:
            continue
        priorEvtCodes.append(t.evtcode)
        if tfirst:
            tfirst = False
        fout.write('        '+t.evtcode+',\n')
    fout.write('        127\n')

fout.write('    }\n};\n')

fout.close()
