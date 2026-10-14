#!/bin/bash
# Common setup sourced by the run scripts.
#   - repo, bladegps, csv_dir, nav: paths in this repository
#   - make_run_dir <name>: create work/<date>_<name>/ and cd into it
#   - run_bladegps <args...>: run bladegps there and keep the console output in bladegps.log

repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
bladegps=$repo/bladegps
csv_dir=$repo/csv
nav=${NAV:-$repo/brdc0150.24n}

if [ ! -x "$bladegps" ]; then
    echo "bladegps is not built. Run 'make' in $repo first."
    exit 1
fi

# Restore the terminal and set TX gains at the minimum when interrupted.
trap 'stty sane 2>/dev/null; bladeRF-cli -e "set gain tx1 -22; set gain tx2 -22" >/dev/null 2>&1' INT TERM

make_run_dir() {
    run_dir=$repo/work/$(date +'%Y%m%d%H%M%S')_$1
    mkdir -p "$run_dir"
    cd "$run_dir" || exit 1
    echo "Run directory: $run_dir"
}

run_bladegps() {
    echo "bladegps $*" > command.txt
    "$bladegps" "$@" 2>&1 | tee bladegps.log
    return "${PIPESTATUS[0]}"
}
