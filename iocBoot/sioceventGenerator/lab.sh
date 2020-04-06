#!/bin/sh

EVG_ADDRESS="${EVG_ADDRESS=192.168.1.129}"
case "$#" in
    1)  EVG_ADDRESS="$1" ;;
    *)
esac
export EVG_ADDRESS
export AUTOSAVE_PATH="\$(TOP)/autosave"
./st.cmd
