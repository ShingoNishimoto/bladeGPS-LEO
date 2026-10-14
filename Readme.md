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
> nearby. Connect the bladeRF TX port to the receiver with a cable and attenuators.
> Calibrate the power for your cabling before using a receiver (see [Power calibration](#power-calibration)).

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
(check the warning above, and calibrate the TX gain for your cabling first, see
[Power calibration](#power-calibration)):

    TX_GAIN=-20 scripts/leo.sh

`q` or Ctrl-C stops it. Without a script, the same run is:

    mkdir -p work/test && cd work/test
    ../../bladegps -t 2024/01/15,05:00:00 -e ../../brdc0150.24n -u ../../csv/leo_full_states_eci.csv -j \
        -d 8000 -s ./ -a -20 -r 0,90 -I -v

## Scripts

The scripts in `scripts/` create a run directory `work/<date>_<name>/`, run bladegps there and
keep the logs, the command line (`command.txt`) and the console output (`bladegps.log`).
`work/` is in git, but its contents are not. When interrupted, the TX gains are set to the minimum.

| Script | Scenario |
| --- | --- |
| `scripts/static.sh` | Static ground receiver on TX1 |
| `scripts/leo.sh` | LEO spacecraft on TX1, with the antenna patterns and path loss |
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
  -E               Equal signal power for all satellites (no antenna patterns, drop below 18 dB-Hz)
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

### Signal power of each satellite

bladeGPS models the C/N0 of each satellite from the GPS transmit power and antenna pattern,
the free-space path loss and the receiver antenna (pattern and G/T), and logs it in `cn0_chN.txt`.
How it is applied to the transmitted signals depends on `-E` and `-p`:

| Options | Relative signal power of the satellites |
| --- | --- |
| (none) | Follows the modeled C/N0: GPS Tx antenna pattern, Rx antenna pattern (off-boresight angle w.r.t. `-r`/`-R`) and path loss |
| `-p` | As above but without the path loss |
| `-E` | Equal for all satellites (with path loss unless `-p`). Satellites below 18 dB-Hz are not transmitted |

The level of each channel is relative: 0 dB is the full amplitude (all satellites with `-E -p`,
or otherwise a satellite at 20200 km with the strongest GPS antenna gain, on the Rx antenna boresight). `-v` prints the level of each
channel in dB. The absolute power at the receiver is set by `-a` and your cabling, see
[Power calibration](#power-calibration).

### Power calibration

The power at the receiver input depends on the TX gain `-a`, the bladeRF and the cables,
attenuators and splitters between the bladeRF and the receiver. Calibrate it for each setup before
running with an actual GNSS receiver, so that the C/N0 the receiver reports matches the modeled
C/N0 in `cn0_chN.txt`.

1. Connect TX1 (and TX2) to the receiver RF input with a cable and fixed attenuators. If the
   receiver supplies a DC bias for an active antenna, put a DC block in between to protect the
   bladeRF. Start with a large attenuation and a low TX gain, and increase step by step.

2. Run a static scenario with equal power for all satellites (`-E -p`, as in `scripts/static.sh`),
   so that every satellite is transmitted at the level 0 dB:

        TX_GAIN=-20 scripts/static.sh

   bladeGPS prints the C/N0 that corresponds to the level 0 dB in the link budget model:

        Reference C/N0 (signal level 0 dB): 50.6 dB-Hz

3. Read the C/N0 of the tracked satellites from the receiver (they are about the same) and change
   `-a` (or the attenuation) until they match the reference C/N0. 1 dB of `-a` changes the C/N0 by
   about 1 dB. Check the linearity: increasing `-a` by 3 dB should increase the C/N0 by 3 dB. If it
   does not, the receiver front end saturates; add attenuation.

4. Keep this TX gain for the setup (e.g. `TX_GAIN=<gain>` for the scripts). Then, without `-E` and
   `-p`, the C/N0 of each satellite at the receiver follows `cn0_chN.txt`, and with `-E -p` all
   satellites are received at about the reference C/N0.

Notes:

* The reference (50.6 dB-Hz) assumes a 0 dBi receiver antenna at boresight and G/T of -24 dB/K. If your receiver
  cannot reach it (e.g. its noise floor or the bladeRF noise limits the C/N0), calibrate to a lower
  C/N0 and take the offset into account when comparing with `cn0_chN.txt`.
* TX1 and TX2 use the same `-a`, so equalize the two paths with attenuators, and check both.
* Calibrate again when the cables, attenuators, bladeRF or receiver change.

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
| `cn0_chN.txt` | Modeled C/N0 [dB-Hz] of each allocated channel at 10 Hz |

## Embedding

`make lib` builds `libbladegps.so`, which exposes

    int bladegps_main(struct bladerf *dev, int argc, char *argv[]);

with the same options as the executable (`argv[0]` is skipped). Given an opened device, bladeGPS
uses the sample rate already set on it (e.g. the same as a receiver on the same bladeRF) and does
not close it. With `dev == NULL`, bladeGPS opens the device (`-b`), sets 2.6 Msps and closes it
on exit. This is how [gnss_sim_receiver](https://github.com/ShingoNishimoto/gnss_sim_receiver)
runs bladeGPS together with GNSS-SDR.

## Docker

Instead of installing libbladeRF on the host, bladeGPS can run in a Docker container (Ubuntu
22.04 with libbladeRF built from the [bladeRF](https://github.com/Nuand/bladeRF) release
`2023.02`; another release can be set with `--build-arg BLADERF_VERSION=<tag>`).

1. [Install Docker Engine](https://docs.docker.com/engine/install/ubuntu/) with the Compose
   plugin, and add your user to the `docker` group (`sudo usermod -aG docker $USER`, then log in again).

2. Build the image from the repository root:

        docker compose -f docker/docker-compose.yml build

3. Run a script in the container. The logs are written to `work/` of the repository on the host,
   owned by your user:

        export HOST_UID=$(id -u) HOST_GID=$(id -g)
        docker compose -f docker/docker-compose.yml run --rm bladegps scripts/leo.sh -f
        docker compose -f docker/docker-compose.yml run --rm -e TX_GAIN=-20 bladegps scripts/leo.sh

   Environment variables of the scripts are given with `-e`, and options of bladegps after the
   script name. Without a command, `scripts/leo.sh -f` is run.

The container accesses the bladeRF through `/dev/bus/usb` in privileged mode, with the `plugdev`
group of the host (gid 46 on Ubuntu; set `PLUGDEV_GID` if different) for the device permission,
so the udev rules of libbladeRF must be installed on the host (see Setup). The same with
`docker run`:

    docker build -f docker/Dockerfile -t bladegps .
    docker run --rm -it --privileged -v /dev/bus/usb:/dev/bus/usb \
        --user $(id -u):$(id -g) --group-add $(getent group plugdev | cut -d: -f3) \
        -v /etc/localtime:/etc/localtime:ro -v $PWD/work:/opt/bladeGPS/work \
        bladegps scripts/leo.sh -f

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
