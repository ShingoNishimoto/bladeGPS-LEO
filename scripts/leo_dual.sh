#!/bin/bash
# Two LEO spacecraft receivers with different orbits on TX1 and TX2 (ECI user motion, -j).
# Usage: TRAJ2=<second orbit>_eci.csv scripts/leo_dual.sh [extra bladegps options, e.g. -f]
# Overridable: NAV, START, DURATION, TRAJ1, TRAJ2, TX_GAIN

source "$(dirname "$0")/common.sh"

# START must be the epoch (UTC) both trajectories were generated for.
START=${START:-2024/01/15,05:00:00}
DURATION=${DURATION:-8000}
TRAJ1=${TRAJ1:-$csv_dir/leo_full_states_eci.csv}
TRAJ2=${TRAJ2:-$csv_dir/leo_states_eci.csv}
TX_GAIN=${TX_GAIN:--15}

for f in "$TRAJ1" "$TRAJ2"; do
    if [ ! -f "$f" ]; then
        echo "Trajectory not found: $f"
        echo "Only one example orbit is in csv/. Give the second one with TRAJ2=<file>."
        exit 1
    fi
done

make_run_dir leo_dual
run_bladegps -t "$START" -e "$nav" -d "$DURATION" -u "$TRAJ1" -U "$TRAJ2" -j \
    -s ./ -a "$TX_GAIN" -r 0,90 -R 0,90 -p -I -E -v "$@"
