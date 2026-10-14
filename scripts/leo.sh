#!/bin/bash
# LEO spacecraft receiver on TX1, user motion in ECI (-j).
# Usage: scripts/leo.sh [extra bladegps options, e.g. -f]
# Overridable: NAV, START, DURATION, TRAJ, TX_GAIN

source "$(dirname "$0")/common.sh"

# START must be the epoch (UTC) the trajectory was generated for.
START=${START:-2024/01/15,05:00:00}
DURATION=${DURATION:-8000}
TRAJ=${TRAJ:-$csv_dir/leo_full_states_eci.csv}
TX_GAIN=${TX_GAIN:--15}

make_run_dir leo
run_bladegps -t "$START" -e "$nav" -d "$DURATION" -u "$TRAJ" -j \
    -s ./ -a "$TX_GAIN" -r 0,90 -p -I -E -v "$@"
