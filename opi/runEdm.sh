#!/bin/sh

prefix="EVG:"
edl="EVG_Engineering.edl"

for i in "$@"
do
    case "$i" in
    *.edl)  edl="$i" ;;
    *)      prefix="$i" ;;
    esac
done
P=`echo "$prefix" | sed -ne '/\(.\).*/s//\1/p'`
R=`echo "$prefix" | sed -ne '/.\(.*\)/s//\1/p'`

edm -eolc -x -m "P=$P,R=$R" "$edl" &
