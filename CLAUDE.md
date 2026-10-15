# CLAUDE.md

Context for continuing development of bladeGPS-LEO. See `Readme.md` for usage.

## Project

Real-time GPS L1 C/A signal simulator for bladeRF, for LEO spacecraft (and ground) receivers.
Derived from osqzss/bladeGPS (remote `upstream`), developed in the gnss_sim_receiver project
(where bladeGPS is embedded in GNSS-SDR), and made standalone here. Scope is LEO: Moon
occultation, SPICE and the GNSS-SDR environment library were removed on purpose.

## Working rules

- Ask the user before every commit and push (including amends and force-pushes): show the changes
  and the proposed message first.
- Never add local information (absolute paths, device serial numbers, host names, accounts) to
  the repository.
- Only `csv/leo_full_states_eci.csv` is tracked in `csv/`; other trajectories stay local.
- `work/` holds run outputs; only `work/.gitignore` is tracked.
- Commit messages: imperative summary line, body explaining what and why, plus how it was tested.
- RF tests: TX gain at the minimum (-22 dB) and short durations unless the user says otherwise.
  The TX port must be cabled to the receiver through attenuators.

## Layout

| Path | Content |
| --- | --- |
| `bladegps.c/h` | Options, bladeRF device setup, TX thread, `bladegps_main()` entry point |
| `gpssim.c/h` | Signal simulation (from gps-sdr-sim): ephemeris, visibility, ranges, nav message, IQ |
| `gpssatellite.c/h` | GPS satellite table and Tx antenna patterns (`gnss/`) |
| `eci.c/h` | Simple ECI (z-rotation) <-> ECEF, IAU 1982 GMST |
| `main.c` | Standalone executable: `bladegps_main(NULL, ...)` |
| `scripts/` | Run scripts (`common.sh` + scenarios); each run goes to `work/<date>_<name>/` |
| `docker/` | Dockerfile (libbladeRF from a pinned release) and compose runner |
| `gnss/` | `gps_table.csv` (PRN/SVN/antenna file) and antenna patterns (zenith x azimuth, dBi) |

Build: `make` (executable), `make lib` (`libbladegps.so`). Dependencies: libbladeRF, libm, pthread.

## Design decisions and conventions

- **Time**: `-t`/`-T` are UTC. `utc2gps()`/`gps2utc()` use the leap second table;
  `date2gps()`/`gps2date()` are plain GPS-calendar conversions used for RINEX epochs (already GPS
  time). Logs use GPS time of week.
- **User motion**: 10 Hz rows `c0,x,y,z,vx,vy,vz`; row i is played at `g0 + 0.1 i` (c0 is not a
  time). Default ECEF. With `-j` (both channels) the state is ECI and c0 is the rotation angle.
- **ECI**: pseudo-inertial frame = ECEF rotated about z by theta only (no precession/nutation),
  as the S2E "SIMPLE" Earth rotation that generated the example trajectories.
  `v_ecef = Rz(theta) v_eci - omega x r_ecef`, omega = `OMEGA_EARTH`.
  The angle in the example CSVs is GMST(UTC + 67.184 s), not IAU 1982 GMST at the UTC epoch
  (cause not found; 67.184 = 32.184 + 35 suggests GMST on a TT-based date with old TAI-UTC).
  So the ECI log uses the file angle with `-j`, and computed GMST otherwise.
- **Two channels**: Ch1 -> TX1, Ch2 -> TX2; any of `-U`/`-L`/`-R` enables Ch2. Both use `-a`.
- **Signal power** (`computeObservation`): `chan->gain` is an amplitude scaled by
  2^`SIGNAL_GAIN_SHIFT` (15) = path loss (20200 km reference / d) x Rx pattern (10^(-dB/20)) x
  Tx pattern (10^((G - Gmax)/20), Gmax = max over all patterns). The modeled C/N0 uses the same
  terms in dB with a 0 dBi Rx antenna and G/T -24 dB/K, so C/N0 - signal level [dB] is constant
  (`referenceCN0()`, printed at start-up, 50.6 dB-Hz). `-E`: equal amplitude, satellites below
  18 dB-Hz dropped. `-p`: no path loss in the signal (C/N0 still includes it).
- **TX stream**: libbladeRF queue `NUM_BUFFERS x SAMPLES_PER_BUFFER` = 32 x 32768 (0.4 s);
  generation leads RF by ~0.6 s. Build with `CPPFLAGS="-DNUM_BUFFERS=512 -DNUM_TRANSFERS=64"`
  when embedded in a heavy process (gnss_sim_receiver used 512 against instability). At the end,
  bladeGPS waits for the queue to drain before disabling TX.
- **Device**: `bladegps_main(dev)` with a caller-owned device uses its sample rate and does not
  close it; with `NULL` it opens `-b` (bare serial prefix or `*:serial=...`), sets 2.6 Msps,
  and closes it. Fast mode (`-f`) never touches the device.
- **Data directory**: `-D` > `$BLADEGPS_DATA_DIR` > `GNSS_DATA_DIR` (build time) > `./gnss`.
- stdout is line buffered (scripts pipe through `tee`); real-time mode prints progress every 1 s.

## History (since upstream `ed7f2e3`)

1. Fast-forwarded to gnss_sim_receiver's bladeGPS `880407f` (LEO features, antenna patterns,
   two channels, fast mode, iterative transmission time).
2. Bugs fixed: integer antenna cells read as NaN/0 dB (libcsv union misuse); scenario end took the
   error path (TX thread never ended); CN0 log crash without `-s`; default Rx antenna elevation
   not set; `-t` converted to UTC - 1 s instead of UTC + 18 s and RINEX TOC shifted 1 s; Tx pattern
   applied twice in dB and normalized per satellite; coarse int gain (vanished below -42 dB);
   3.2 s TX queue cut the end of the RF output; output not shown when piped.
3. Removed gnss-sdr/SPICE/libcsv/OpenMP dependencies; standalone build, ECI support, scripts,
   Docker, README for new users, power calibration procedure.

## Verification methods

- **Regression against a reference build**: build a commit (e.g. `880407f` with SPICE, or `HEAD`)
  in a scratch directory and run both in fast mode with the same scenario
  (`-t 2024/01/15,05:00:00 -e brdc0150.24n -u <LEO> -L <ground> -d 8000 -s <dir> [-E -p -I]`),
  then `cmp` the logs (`visibility_chN`, `cn0_chN`, `user_states_chN`). A harness that fills
  `sim_t` and calls `gps_task()` directly runs code that has no fast-mode-safe `bladegps_main`.
  Note that the time fix shifts `-t` by 19 s compared with commits before it.
- **ECI**: `*_eci.csv` -> ECEF vs `*_ecef.csv` pairs (3e-8 m, 1e-5 m/s); GMST vs Vallado Ex. 3-5
  (152.578787810 deg) and J2000.0 (280.46061837 deg).
- **Signal power**: instrument `computeObservation` to print C/N0 and 20 log10(gain / 2^15); their
  difference is constant over satellites (within 0.01 dB) without `-p`.
- **RF**: wall time from "Running..." to "Done!" equals the duration; no `TX failed` messages.

## Open items

- Receiver side (GNSS-SDR, not in this repo): for LEO, acquisition needs `doppler_max` ~50 kHz
  (Doppler up to about +-35 kHz); with +-5 kHz only near-zenith satellites are acquired.
  A larger `doppler_step` (about 500 Hz for 1 ms coherent integration) speeds it up.
- Possible: add Doppler to the `-v` channel table.
- Tx antenna gain per channel is truncated to int8 dB (`GetAntennaGain`), and only azimuth 0 of
  the patterns is used (satellite attitude not modeled).
- The `-v` channel table is printed after the 30 s re-allocation, so new channels show stale values.
- Windows/macOS builds are not maintained (POSIX `getline`, `usleep`).
- Power calibration against a receiver: the reference C/N0 has not been checked on hardware yet.
