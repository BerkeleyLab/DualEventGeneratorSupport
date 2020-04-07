#!/bin/sh

# Track operation of MRF event generator IOC

camonitor -tsi -g9 -#4 \
    "LI11:EVG1-SoftSeq:0:Timestamp-SP" \
    "LI11:EVG1-SoftSeq:0:EvtCode-SP" \
    "LI11:EVG1-SoftSeq:0:Enable-RB" \
    "LI11:EVG1-SoftSeq:0:Commit-Cmd" \
    "LI11:EVG1-SoftSeq:0:LoadedSeq-RB" \
    "LI11:EVG1-SoftSeq:1:Enable-Cmd" \
    "LI11:EVG1-SoftSeq:1:Disable-Cmd"
