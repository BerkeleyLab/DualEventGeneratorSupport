#!/bin/sh

T=""
edl="TIM_injectionSequencer.edl"

for i in "$@"
do
    case "$i" in
    *.edl)  edl="$i" ;;
    *)      T="$i" ;;
    esac
done

case "$T" in
    "") TARG="T=\"$T\"" ;;
    *)  TARG="T=$T"     ;;
esac

edm -eolc -x -m $TARG "$edl" &
