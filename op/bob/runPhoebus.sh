#!/bin/bash

set -eu

SCRIPTPATH="$( cd "$( dirname "${BASH_SOURCE[0]}"  )" && pwd  )"

prefix="EVG:"
bob="EVG_Engineering.bob"

for i in "$@"
do
    case "$i" in
    *.bob)  bob="$i" ;;
    *)      prefix="$i" ;;
    esac
done
P=`echo "$prefix" | sed -ne '/\(.\).*/s//\1/p'`
R=`echo "$prefix" | sed -ne '/.\(.*\)/s//\1/p'`

phoebus -resource file:${SCRIPTPATH}/autoconvert/${bob}?"P=${P}&R=${R}"
