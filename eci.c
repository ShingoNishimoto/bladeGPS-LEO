#include <math.h>

#include "eci.h"

#define JD_GPS_EPOCH 2444244.5  // 1980/01/06 00:00:00
#define JD_J2000 2451545.0      // 2000/01/01 12:00:00
#define DAYS_IN_JULIAN_CENTURY 36525.0

double gpsTimeToGmst(gpstime_t g)
{
    // Days since J2000 in UT1 (= UTC, i.e. UT1-UTC is neglected).
    // Keep the integer days and the fraction apart for precision.
    const double sec_utc = g.sec - SECONDS_UTC_TO_GPS;
    const double days_whole = (double)g.week * 7.0 + floor(sec_utc / SECONDS_IN_DAY);
    const double day_frac = (sec_utc - floor(sec_utc / SECONDS_IN_DAY) * SECONDS_IN_DAY) / SECONDS_IN_DAY;
    const double du = (days_whole + (JD_GPS_EPOCH - JD_J2000)) + day_frac;
    const double tu = du / DAYS_IN_JULIAN_CENTURY;

    // IAU 1982 GMST [s]. 876600 h = 36525 days of 24 h.
    double gmst_sec = 67310.54841
                    + (876600.0 * SECONDS_IN_HOUR + 8640184.812866) * tu
                    + 0.093104 * tu * tu
                    - 6.2e-6 * tu * tu * tu;
    gmst_sec = fmod(gmst_sec, SECONDS_IN_DAY);
    if (gmst_sec < 0.0)
        gmst_sec += SECONDS_IN_DAY;

    return gmst_sec / SECONDS_IN_DAY * 2.0 * PI;
}

void eci2ecef(double theta, const double *eci, double *ecef)
{
    const double c = cos(theta);
    const double s = sin(theta);

    double r[3], v[3];
    r[0] =  c * eci[0] + s * eci[1];
    r[1] = -s * eci[0] + c * eci[1];
    r[2] = eci[2];

    // v_ecef = R * v_eci - omega x r_ecef
    v[0] =  c * eci[3] + s * eci[4] + OMEGA_EARTH * r[1];
    v[1] = -s * eci[3] + c * eci[4] - OMEGA_EARTH * r[0];
    v[2] = eci[5];

    for (int i = 0; i < 3; i++)
    {
        ecef[i] = r[i];
        ecef[3 + i] = v[i];
    }
}

void ecef2eci(double theta, const double *ecef, double *eci)
{
    const double c = cos(theta);
    const double s = sin(theta);

    // v_eci = R^T * (v_ecef + omega x r_ecef)
    const double vx = ecef[3] - OMEGA_EARTH * ecef[1];
    const double vy = ecef[4] + OMEGA_EARTH * ecef[0];

    double r[3], v[3];
    r[0] = c * ecef[0] - s * ecef[1];
    r[1] = s * ecef[0] + c * ecef[1];
    r[2] = ecef[2];
    v[0] = c * vx - s * vy;
    v[1] = s * vx + c * vy;
    v[2] = ecef[5];

    for (int i = 0; i < 3; i++)
    {
        eci[i] = r[i];
        eci[3 + i] = v[i];
    }
}
