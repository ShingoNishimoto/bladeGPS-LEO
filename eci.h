#ifndef ECI_H
#define ECI_H

#include "gpssim.h"

/*
 * Simple ECI <-> ECEF conversion by a rotation about the z axis.
 *
 * The ECI frame here is a pseudo-inertial frame rotated from ECEF by the
 * Earth rotation angle theta only. Precession, nutation and polar motion are
 * not modeled, so it is not J2000/GCRF. This is the same convention as the
 * *_eci.csv / *_ecef.csv trajectory pairs, whose first column is theta [rad].
 *
 * State vectors are {x, y, z, vx, vy, vz} in [m] and [m/s].
 */

/*! \brief Greenwich mean sidereal time (IAU 1982) [rad] at the GPS time, assuming UT1 = UTC */
double gpsTimeToGmst(gpstime_t g);

/*! \brief Convert an ECI state to ECEF with the Earth rotation angle theta [rad] */
void eci2ecef(double theta, const double *eci, double *ecef);

/*! \brief Convert an ECEF state to ECI with the Earth rotation angle theta [rad] */
void ecef2eci(double theta, const double *ecef, double *eci);

#endif // ECI_H
