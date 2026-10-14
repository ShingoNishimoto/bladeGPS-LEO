#!/bin/bash
# Static ground receiver on TX1.
# Usage: scripts/static.sh [extra bladegps options, e.g. -f]
# Overridable: NAV, START, DURATION, LOCATION, TX_GAIN

source "$(dirname "$0")/common.sh"

START=${START:-2024/01/15,05:00:00}
DURATION=${DURATION:-3000}
LOCATION=${LOCATION:-38.257998389397734,140.83660460172888,158.6}
TX_GAIN=${TX_GAIN:--15}

make_run_dir static
run_bladegps -t "$START" -e "$nav" -d "$DURATION" -l "$LOCATION" \
    -s ./ -a "$TX_GAIN" -r 0,90 -p -I -E -v "$@"
