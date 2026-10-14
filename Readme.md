# bladeGPS-LEO

Real-time GPS L1 C/A signal simulator for bladeRF, extended for LEO spacecraft scenarios.

It generates the GPS signals that a receiver on a ground station or on a LEO spacecraft would see,
and transmits them with a bladeRF in real time. It can also run only the simulation (no hardware)
to produce the satellite visibility and C/N0 logs.

This is a derivative of [bladeGPS](https://github.com/osqzss/bladeGPS) by Takuji Ebinuma
(based on [gps-sdr-sim](https://github.com/osqzss/gps-sdr-sim)), developed further in
[gnss_sim_receiver](https://github.com/ShingoNishimoto/gnss_sim_receiver) and made standalone here.

Main differences from the original bladeGPS:

* User motion with position and velocity at 10 Hz, in ECEF or in a simple ECI frame (`-j`)
* Visibility check for spacecraft above the ionosphere (Earth blockage with a 1000 km exclusion zone)
* Iterative signal transmission time and Earth rotation correction
* GPS transmit antenna patterns per satellite (`gnss/antenna/`), path loss and C/N0 based satellite selection
* Up to 24 channels, and two TX channels for two receivers (e.g. two LEO spacecraft, or ground + LEO)
* Fast mode (`-f`): simulation and logs only, without a bladeRF
* Logs of user states (ECEF/ECI), visibility and C/N0 (`-s`)

Moon occultation and other cislunar features of gnss_sim_receiver are not included.

> [!WARNING]
> Transmitting on GPS L1 over the air is illegal in most countries and can disturb GPS receivers
> nearby. Connect the bladeRF TX port to the receiver with a cable and attenuators, or use a
> shielded enclosure.

## Requirements

* Linux (tested on Ubuntu 22.04)
* gcc, make, pkg-config
* [libbladeRF](https://github.com/Nuand/bladeRF) 2.x
* bladeRF 2.0 micro (tested; bladeRF x40/x115 should also work) with its FPGA loaded.
  Only needed for RF output, not for fast mode.

## Setup

1. Install libbladeRF and the bladeRF tools, either from Nuand's PPA

        sudo add-apt-repository ppa:nuandllc/bladerf
        sudo apt update
        sudo apt install bladerf libbladerf-dev

   or [from source](https://github.com/Nuand/bladeRF/wiki/Getting-Started:-Linux)
   (`cmake .. && make && sudo make install && sudo ldconfig` in `host/build`).

2. Check that the device is found and the FPGA is loaded. If it is not loaded, download the
   FPGA image for your board from [Nuand](https://www.nuand.com/fpga_images/) and store it in
   the flash with `bladeRF-cli -L <image>.rbf` (once).

        bladeRF-cli -p
        bladeRF-cli -e info

   If the device is found only with `sudo`, install the udev rules of libbladeRF
   (`host/misc/udev/88-nuand-*.rules` in the bladeRF source) to `/etc/udev/rules.d/`.

3. Clone and build

        git clone https://github.com/ShingoNishimoto/bladeGPS-LEO.git
        cd bladeGPS-LEO
        make

   This builds `./bladegps`. The data directory `gnss/` (GPS satellite table and antenna
   patterns) is compiled in as an absolute path, so build in the place you will keep the
   repository, or use `-D <dir>` / `$BLADEGPS_DATA_DIR`.

   If libbladeRF is not installed but only built in a bladeRF source tree:

        make BLADERF_CFLAGS=-I<bladeRF>/host/libraries/libbladeRF/include \
             BLADERF_LIBS="-L<bladeRF>/host/build/output -lbladeRF"

## Quick start

Simulation only (no bladeRF needed), LEO spacecraft from the example trajectory:

    scripts/leo.sh -f

This creates `work/<date>_leo/` with the logs and finishes in a few seconds. Then with RF output
(check the warning above and set the TX gain for your attenuation):

    TX_GAIN=-20 scripts/leo.sh

`q` or Ctrl-C stops it. Without a script, the same run is:

    mkdir -p work/test && cd work/test
    ../../bladegps -t 2024/01/15,05:00:00 -e ../../brdc0150.24n -u ../../csv/leo_full_states_eci.csv -j \
        -d 8000 -s ./ -a -20 -r 0,90 -p -I -E -v

## Scripts

The scripts in `scripts/` create a run directory `work/<date>_<name>/`, run bladegps there and
keep the logs, the command line (`command.txt`) and the console output (`bladegps.log`).
`work/` is in git, but its contents are not. When interrupted, the TX gains are set to the minimum.

| Script | Scenario |
| --- | --- |
| `scripts/static.sh` | Static ground receiver on TX1 |
| `scripts/leo.sh` | LEO spacecraft on TX1 |
| `scripts/leo_dual.sh` | Two LEO spacecraft with different orbits on TX1 and TX2 (set the second orbit with `TRAJ2`) |
| `scripts/ground_leo.sh` | Static ground receiver on TX1 and LEO spacecraft on TX2 |
| `scripts/realtime.sh` | Static receiver at the current time with today's ephemeris from CDDIS (`-l` required) |

Extra arguments are passed to bladegps (e.g. `-f` for simulation only), and the main parameters
can be overridden by environment variables, for example:

    TX_GAIN=-20 DURATION=600 scripts/leo.sh
    TRAJ2=/path/to/second_orbit_eci.csv scripts/leo_dual.sh -f
    scripts/realtime.sh -l 35.274,137.014,100 -d 600

`realtime.sh` downloads the broadcast ephemeris from CDDIS, which needs an
[Earthdata](https://urs.earthdata.nasa.gov) login in `~/.netrc`
(`machine urs.earthdata.nasa.gov login <user> password <password>`).

## Options

```
Usage: bladegps [options]
Options:
  -e <gps_nav>     RINEX navigation file for GPS ephemerides (required)
  -y <yuma_alm>    YUMA almanac file for GPS almanacs
  -u <user_motion> User motion file (dynamic mode) for Ch1
  -U <user_motion> User motion file (dynamic mode) for Ch2
  -s <log_dir>     Log directory of user position (dynamic mode)
  -g <nmea_gga>    NMEA GGA stream (dynamic mode)
  -l <location>    Lat,Lon,Hgt (static mode) e.g. 35.274,137.014,100
  -L <location>    Lat,Lon,Hgt (static mode) of Ch2 e.g. 35.274,137.014,100
  -t <date,time>   Scenario start time in UTC YYYY/MM/DD,hh:mm:ss
  -T <date,time>   Overwrite TOC and TOE to scenario start time in UTC (or 'now')
  -d <duration>    Duration [sec] (max: 86400)
  -x <XB number>   Enable expansion board, e.g. '-x 200' for XB200
  -b <devstr>      bladeRF device string (standalone use, default: first device)
  -D <data_dir>    Directory of gps_table.csv and antenna/ (default: $BLADEGPS_DATA_DIR or ./gnss)
  -a <tx_gain>     TX Gain (default: -5)
  -r <azi,ele>     Rx antenna attitude in degree (default: azi=0, ele=90)
  -R <azi,ele>     Rx antenna attitude in degree of Ch2 (default: same as Ch1)
  -i               Interactive mode: North='w', South='s', East='d', West='a'
  -I               Disable ionospheric delay for spacecraft scenario
  -E               Equal distribution of rx and tx antenna pattern
  -p               Disable path loss and hold power level constant
  -j               User motion files are in ECI: angle[rad],x,y,z,vx,vy,vz (z-rotation by angle from ECEF)
  -f               Fast option, run only simulation part as fast as possible without generating IQ (no device needed).
  -v               Verbosity option
```

* Ch1 is transmitted on TX1 and Ch2 on TX2. Giving any Ch2 option (`-U`, `-L`, `-R`) enables TX2.
* The scenario start time must be within the RINEX navigation file (`-T` shifts the ephemerides
  to the start time instead).
* `-j` applies to both channels.
* `-a` is the bladeRF TX gain [dB] (bladeRF 2.0: -23.75 to 66). Both TX channels use the same gain.

## Inputs

### Ephemeris

RINEX 2 GPS navigation file, e.g. the daily `brdcDDD0.YYn` from
[CDDIS](https://cddis.nasa.gov/archive/gnss/data/daily/). `brdc0150.24n` (2024/01/15) is included
for the example trajectory.

### User motion file

CSV without a header, one row every 0.1 s, starting at the scenario start time (`-t`):

    c0, x, y, z, vx, vy, vz

Position in m and velocity in m/s. By default the state is ECEF and `c0` is not used.
With `-j` the state is ECI and `c0` is the rotation angle theta [rad] from ECEF to ECI at that row.

### Example trajectory (`csv/`)

`csv/leo_full_states_eci.csv`: LEO at about 410 km, 8000 s at 10 Hz from 2024/01/15 05:00:00 UTC,
in ECI with theta (use with `-j`). It was generated by [S2E](https://github.com/ut-issl/s2e-core)
with the simple (z-rotation only) Earth rotation. Other CSV files in `csv/` are not tracked by git.

### ECI frame

The ECI frame is rotated from ECEF about the z axis by theta only:
`r_ecef = Rz(theta) r_eci`, `v_ecef = Rz(theta) v_eci - omega x r_ecef`.
Precession, nutation and polar motion are not modeled, so it is not J2000/GCRF.

With `-s`, `user_states_eci_chN.txt` is written beside `user_states_chN.txt`. Its theta is
taken from the user motion file with `-j`, otherwise it is the IAU 1982 GMST at the simulation
time (UT1 = UTC). Note that theta in the example trajectory is about 67 s of Earth rotation ahead of
the IAU 1982 GMST at its UTC time, so use `-j` when the ECI log must match an ECI trajectory.

### Time

`-t`/`-T` are in UTC and converted to GPS time with the leap seconds (GPS - UTC = 18 s since 2017).
The simulation itself runs in GPS time, and the logs use the GPS time of week [s].

## Logs (`-s <log_dir>`)

| File | Content |
| --- | --- |
| `user_states_chN.txt` | GPS time of week, c0, ECEF position and velocity (dynamic mode) |
| `user_states_eci_chN.txt` | GPS time of week, theta, ECI position and velocity (dynamic mode) |
| `visibility_chN.txt` | Visibility (0/1) of each PRN at every channel allocation (30 s) |
| `cn0_chN.txt` | C/N0 [dB-Hz] of each channel at 10 Hz (only with `-E`) |

## Embedding

`make lib` builds `libbladegps.so`, which exposes

    int bladegps_main(struct bladerf *dev, int argc, char *argv[]);

with the same options as the executable (`argv[0]` is skipped). Given an opened device, bladeGPS
uses the sample rate already set on it (e.g. the same as a receiver on the same bladeRF) and does
not close it. With `dev == NULL`, bladeGPS opens the device (`-b`), sets 2.6 Msps and closes it
on exit. This is how [gnss_sim_receiver](https://github.com/ShingoNishimoto/gnss_sim_receiver)
runs bladeGPS together with GNSS-SDR.

## Docker (untested)

    docker build -f docker/Dockerfile -t bladegps .
    docker run -it --rm --privileged -v /dev/bus/usb:/dev/bus/usb bladegps \
        /bin/bash -c "bladegps -e /opt/bladeGPS/brdc0150.24n -u /opt/bladeGPS/csv/leo_full_states_eci.csv -j -t 2024/01/15,05:00:00 -d 600 -a -20"

## Troubleshooting

| Message | Cause |
| --- | --- |
| `Failed to open device` | bladeRF not connected, used by another program, or no permission (udev rules) |
| `ERROR: Invalid start time.` | `-t` is outside the RINEX navigation file |
| `Failed to open: .../gps_table.csv` | Data directory not found: use `-D <repo>/gnss` |
| `bladegps is not built.` | Run `make` in the repository before the scripts |

## License

Copyright &copy; 2015-2022 Takuji Ebinuma  
Distributed under the [MIT License](http://www.opensource.org/licenses/mit-license.php).
