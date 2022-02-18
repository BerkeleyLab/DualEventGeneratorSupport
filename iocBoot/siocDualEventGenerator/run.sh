#!/bin/sh

# Start soft IOC in various modes
export EVG_ADDRESS="${EVG_ADDRESS=131.243.93.169}"
for i in "$@"
do
    case "$i" in
        [0-9]*)  EVG_ADDRESS="$1" ;;
        -a) export AUTOSAVE_PATH="\$(TOP)/autosave" ;;
        -*) echo "Usage: $0 [-a] [-h] [EVG_ADDRESS]" >&2
            echo "       -a -- Use local autosave/restore directory" >&2
            echo "       -h -- Show this help messdage, then exit" >&2
            exit 1 ;;
    esac
done
export P="testEVG"
export R=":"
./st.cmd
