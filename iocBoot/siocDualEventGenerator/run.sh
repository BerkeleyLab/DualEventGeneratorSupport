#!/bin/sh

# Start soft IOC in various modes
export EVG_ADDRESS="${EVG_ADDRESS=131.243.93.169}"
export FPGA_SIMM_DISABLE="#"
for i in "$@"
do
    case "$i" in
        [0-9]*)  EVG_ADDRESS="$1" ;;
        -s) FPGA_SIMM_DISABLE="" ;;
        -a) export AUTOSAVE_PATH="\$(TOP)/autosave" ;;
        -*) echo "Usage: $0 [-a] [-h] [-s] [EVG_ADDRESS]" >&2
            echo "       -a -- Use local autosave/restore directory" >&2
            echo "       -h -- Show this help messdage, then exit" >&2
            echo "       -s -- Place EVG records into simulation mode" >&2
            exit 1 ;;
    esac
done
export P="testEVG"
export R=":"
./st.cmd
