#!/bin/bash
# Static receiver at the current time with today's broadcast ephemeris from CDDIS.
# Usage: scripts/realtime.sh -l <lat,lon,hgt> [extra bladegps options]
#
# CDDIS requires an Earthdata login (https://urs.earthdata.nasa.gov) in ~/.netrc:
#   machine urs.earthdata.nasa.gov login <user> password <password>
# Downloaded files are kept in work/ephemeris/.

source "$(dirname "$0")/common.sh"

year=$(date -u +%Y)
doy=$(date -u +%j)
brdc=brdc${doy}0.${year: -2}n
eph_dir=$repo/work/ephemeris
mkdir -p "$eph_dir"

if [ ! -e "$eph_dir/$brdc" ]; then
    if [ ! -e "$HOME/.netrc" ]; then
        echo "Set your Earthdata login in ~/.netrc first (see the header of this script)."
        exit 1
    fi
    curl -f -s -S -L -n -c "$eph_dir/.cddis_cookies" -b "$eph_dir/.cddis_cookies" \
        -o "$eph_dir/$brdc.gz" \
        "https://cddis.nasa.gov/archive/gnss/data/daily/$year/brdc/$brdc.gz" || {
        echo "Can't download $brdc.gz"
        exit 1
    }
    gunzip -f "$eph_dir/$brdc.gz"
fi

make_run_dir realtime
run_bladegps -e "$eph_dir/$brdc" -t "$(date -u +'%Y/%m/%d,%H:%M:%S')" -s ./ "$@"
