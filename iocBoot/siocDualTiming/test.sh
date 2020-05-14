#!/bin/sh

# Start soft IOC in test mode
export EVG_ADDRESS="${EVG_ADDRESS=192.168.1.129}"
export FPGA_SIMM_DISABLE="#"
export SEQ_DEBUG="0"
for i in "$@"
do
    case "$i" in
        [0-9][0-9]*.[0-9][0-9]*.[0-9][0-9]*.[0-9][0-9]*.)  EVG_ADDRESS="$1" ;;
        -s) FPGA_SIMM_DISABLE="" ;;
        -d) SEQ_DEBUG="1"
    esac
done
export P="testEVG"
export R=":"
export T="test"
./st.cmd
