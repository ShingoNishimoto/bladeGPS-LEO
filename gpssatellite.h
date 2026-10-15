#ifndef _GPS_SATELLITE_H
#define _GPS_SATELLITE_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// #define ANTENNA_ELE_RESOLUTION_DEG 5
// #define ANTENNA_PAT_ELE_NUM (90 / ANTENNA_ELE_RESOLUTION_DEG + 1)
#define ANT_AZI_NUM (360)  // 0 to 359
#define ANT_ELE_NUM (91)   // 0 to 90
/*! \brief Maximum number of satellites in RINEX file */
#define MAX_SAT (32)

// Source: https://en.wikipedia.org/wiki/GPS_satellite_blocks, 2024/07/25
// typedef enum
// {
//     // Retired now
//     GPS_BLOCK_I,
//     GPS_BLOCK_II,
//     GPS_BLOCK_IIA,
//     // Operational
//     GPS_BLOCK_IIR,
//     GPS_BLOCK_IIRM,
//     GPS_BLOCK_IIF,
//     GPS_BLOCK_III,
//     // Future
//     GPS_BLOCK_IIIF,
//     GPS_BLOCK_MAX
// } GPS_BLOCK;

typedef struct gps_table
{
    uint8_t PRN;
    uint8_t SVN;
    char* block_name;
    char* antenna_file_name;
} gps_table;

typedef struct gpssatellite
{
    gps_table sat_info;
    // int8_t antenna_gain[ANTENNA_PAT_ELE_NUM];  // dB
    double antenna_gain[ANT_ELE_NUM][ANT_AZI_NUM];
    // FIXME: attitude...
} gps_satellite;

uint8_t InitGPSSatellite(gps_satellite* gps_sats);
uint8_t GetAntennaGain(const gps_satellite* gps_sat, const uint8_t elevation_deg, const uint16_t azimuth_deg, int8_t* gain);
uint8_t ReadGpsTable(gps_table* gps_tables);
uint8_t ReadGpsAntennaGain(gps_satellite* gps_sat);
void SetGnssDataDir(const char* dir);

#endif // _GPS_SATELLITE_H
