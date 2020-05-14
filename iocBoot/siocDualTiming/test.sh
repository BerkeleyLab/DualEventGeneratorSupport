#!/bin/sh

# Start soft IOC in test mode
export EVG_ADDRESS="${EVG_ADDRESS=192.168.1.129}"
case "$#" in
    1)  EVG_ADDRESS="$1" ;;
    *)
esac
export FPGA_SIMM_DISABLE=""
export SEQ_DEBUG="0"
export P="testEVG"
export R=":"
export T="test"
./st.cmd
