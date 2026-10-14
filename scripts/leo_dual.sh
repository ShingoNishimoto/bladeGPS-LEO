#!/bin/bash
# Static ground receiver on TX1 and LEO spacecraft receiver on TX2 (ECI user motion, -j),
# as in gnss_sim_receiver/scripts/kinematic_hybrid.sh.
# Usage: scripts/leo_dual.sh [extra bladegps options, e.g. -f]
# Overridable: NAV, START, DURATION, LOCATION, TRAJ, TX_GAIN

source "$(dirname "$0")/common.sh"

# START must be the epoch the trajectory was generated for.
START=${START:-2024/01/15,05:00:00}
DURATION=${DURATION:-8000}
LOCATION=${LOCATION:-38.257998389397734,140.83660460172888,158.6}
TRAJ=${TRAJ:-$csv_dir/leo_full_states_eci.csv}
TX_GAIN=${TX_GAIN:--15}

make_run_dir leo_dual
run_bladegps -t "$START" -e "$nav" -d "$DURATION" -l "$LOCATION" -U "$TRAJ" -j \
    -s ./ -a "$TX_GAIN" -r 0,90 -R 0,90 -p -I -E -v "$@"
