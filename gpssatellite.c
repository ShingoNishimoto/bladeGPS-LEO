#include "gpssatellite.h"

#include <time.h>
#include <math.h>
#ifdef _WIN32
// To avoid conflict between time.h and pthread.h on Windows
#define HAVE_STRUCT_TIMESPEC
#endif

#define MAX_ANTENNA_PATTERN 3

// static const char* block_list[GPS_BLOCK_MAX] = {
//     "I",
//     "II",
//     "IIA",
//     "IIR",
//     "IIR-M",
//     "IIF",
//     "III",
//     "IIIF"
//     };

// NOTE: The gain from 0 - 15 deg is common from the Block IIR data.
// static const int8_t antenna_gains[GPS_BLOCK_MAX][MAX_ANTENNA_PATTERN][ANTENNA_PAT_ELE_NUM] = {
//     // Off-Boresight angle [deg]
//     //  0,  5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90
//     // I
//     {},
//     // II
//     {},
//     // IIA
//     //  0,  5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90
//     { {13, 15, 16, 14,  8,-13, -2, -3,-12,-18, -2,  0,  1, -2, -7,-17,-20,-20,-20},  // pattern 1 default, highest peak
//       {13, 15, 16, 14,  8, -8, -2, -3, -8, -8, -5, -4, -5, -8,-17,-22,-22,-22,-22},  // pattern 2
//       {13, 15, 16, 14,  8, -3, -8,-10,-10,-10,-10,-15,-17,-17,-17,-22,-22,-22,-22},  // pattern 3 lowest peak
//     },
//     // IIR
//     //  0,  5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90
//     { {13, 15, 16, 14,  4,  0,  4,  3, -2,-10, -1,  2,  0, -6,-15,-15,-15,-15,-15},  // pattern 1 default, highest peak
//       {13, 15, 16, 14,  4,  0,  4,  0, -3, -3, -5,-11,-17,-17,-17,-17,-17,-17,-17},  // pattern 2
//       {13, 15, 16, 14,  4,-13, -7, -7, -5, -6, -5, -4, -3, -2, -3, -7,-12,-15,-15},  // pattern 3 lowest peak
//     },
//     // IIR-M
//     //  0,  5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90
//     { {13, 15, 16, 14,  8,  0,  3,  0, -5,-10,-12,-22,-17,-17,-17,-17,-17,-17,-17},  // pattern 1 default, highest peak
//       {13, 15, 16, 14,  8, -5,-15, -7, -3, -5, -4, -3, -8, -8, -6,-15,-20,-14,-14},  // pattern 2
//       {13, 15, 16, 14,  8, -4, -8, -7, -5,-13,-18,-13,-13,-13,-19,-19,-19,-19,-19},  // pattern 3 lowest peak
//     },
//     // IIF
//     //  0,  5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90
//     { {13, 15, 16, 14, -7, -1, -4,-11, -5, -3, -1,  0, -2, -7,-18,-18,-18,-18,-18},  // pattern 1 default, highest peak
//       {13, 15, 16, 14,-13, -5, -4,-11, -7, -5, -4, -7,-11,-19,-19,-19,-19,-19,-19},  // pattern 2
//       {13, 15, 16, 14, -3,-19,-22,-15,-12,-12, -9,-12,-17,-22,-22,-22,-22,-22,-22},  // pattern 3 lowest peak
//     },
//     // III NOTE: there is no information of III, so use that of IIRM because manufacturer is the same (Lockheed Martin)
//     //  0,  5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90
//     { {13, 15, 16, 14,  8,  0,  3,  0, -5,-10,-12,-22,-17,-17,-17,-17,-17,-17,-17},  // pattern 1 default, highest peak
//       {13, 15, 16, 14,  8, -5,-15, -7, -3, -5, -4, -3, -8, -8, -6,-15,-20,-14,-14},  // pattern 2
//       {13, 15, 16, 14,  8, -4, -8, -7, -5,-13,-18,-13,-13,-13,-19,-19,-19,-19,-19},  // pattern 3 lowest peak
//     },
//     // IIIF
//     {}
// };

static char gnss_data_dir[1024] = "";

void SetGnssDataDir(const char* dir)
{
    snprintf(gnss_data_dir, sizeof(gnss_data_dir), "%s", dir);
}

// Priority: SetGnssDataDir() (-D option) > BLADEGPS_DATA_DIR env > GNSS_DATA_DIR at build time > ./gnss
static const char* get_gnss_data_dir(void)
{
    if (gnss_data_dir[0] != '\0')
        return gnss_data_dir;

    const char* env = getenv("BLADEGPS_DATA_DIR");
    if (env != NULL && env[0] != '\0')
        return env;

#ifdef GNSS_DATA_DIR
    return GNSS_DATA_DIR;
#else
    return "./gnss";
#endif
}

static FILE *open_data_file(const char *relative_path) {
    char full_path[2048];
    snprintf(full_path, sizeof(full_path), "%s/%s", get_gnss_data_dir(), relative_path);

    FILE *fp = fopen(full_path, "r");
    if (!fp) {
        perror("fopen");
        fprintf(stderr, "Failed to open: %s\n", full_path);
    }
    return fp;
}

// Split a CSV line in place. Empty fields are kept as empty strings.
static int split_csv_line(char *line, char **fields, int max_fields) {
    line[strcspn(line, "\r\n")] = '\0';

    int n = 0;
    char *p = line;
    while (n < max_fields) {
        fields[n++] = p;
        char *comma = strchr(p, ',');
        if (comma == NULL)
            break;
        *comma = '\0';
        p = comma + 1;
    }
    return n;
}


uint8_t ReadGpsTable(gps_table* gps_tables) {
    FILE *fp = open_data_file("gps_table.csv");
    if (!fp) return 1;

    char line[1024];
    char *fields[5];
    int row = 0;
    int i = 0;
    while (i < MAX_SAT && fgets(line, sizeof(line), fp) != NULL) {
        if (row++ == 0) continue; // header: PRN,SVN,blockName,LM_File,ACE_File
        if (split_csv_line(line, fields, 5) < 5) continue;

        gps_tables[i].PRN = atoi(fields[0]);
        gps_tables[i].SVN = atoi(fields[1]);
        gps_tables[i].block_name = strdup(fields[2]);
        gps_tables[i].antenna_file_name = strdup(fields[4]);
        i++;
    }
    fclose(fp);

    if (i < MAX_SAT) {
        fprintf(stderr, "GPS table has only %d satellites.\n", i);
        return 1;
    }
    return 0;
}

uint8_t ReadGpsAntennaGain(gps_satellite* gps_sat) {
    char rel_path[512];
    snprintf(rel_path, sizeof(rel_path), "antenna/%s.txt", gps_sat->sat_info.antenna_file_name);
    FILE *fp = open_data_file(rel_path);
    if (!fp) return 1;

    // Row: zenith (off-boresight) angle 0-90 deg, column: azimuth 0-359 deg.
    char *line = NULL;
    size_t cap = 0;
    char *fields[ANT_AZI_NUM + 1];
    int row = 0;
    int i = 0;
    while (i < ANT_ELE_NUM && getline(&line, &cap, fp) != -1) {
        if (row++ == 0) continue; // header: azimuth angles
        int n = split_csv_line(line, fields, ANT_AZI_NUM + 1);
        for (int j = 1; j < n; j++) {
            gps_sat->antenna_gain[i][j - 1] = strtod(fields[j], NULL);
        }
        i++;
    }
    free(line);
    fclose(fp);

    if (i < ANT_ELE_NUM) {
        fprintf(stderr, "Antenna pattern %s has only %d rows.\n", rel_path, i);
        return 1;
    }
    return 0;
}



uint8_t InitGPSSatellite(gps_satellite* gps_sats)
{
    gps_table gps_tables[MAX_SAT];
    if (ReadGpsTable(gps_tables))
        return 1;
    for (int i = 0; i < MAX_SAT; i++)
        {
            gps_sats[i].sat_info = gps_tables[i];
            if (ReadGpsAntennaGain(&gps_sats[i]))
                return 1;
        }

    return 0;
}

// TODO: consider the azimuth distribution
uint8_t GetAntennaGain(const gps_satellite* gps_sat, const uint8_t elevation_deg, const uint16_t azimuth_deg, int8_t* gain)
{
    if (elevation_deg > 90 || azimuth_deg >= 360)
    {
        printf("Invalid attitude. Ele: %u, Azi: %u [deg]\n", elevation_deg, azimuth_deg);
        return 1;
    }

    const uint8_t zenith_deg = 90 - elevation_deg;
    *gain = gps_sat->antenna_gain[zenith_deg][azimuth_deg];
    // const uint8_t nearest_id = round((ANTENNA_PAT_ELE_NUM - 1) * zenith_deg / 90.0);
    // const uint8_t nearest_zenith = nearest_id * ANTENNA_ELE_RESOLUTION_DEG;
    // uint8_t other_id;
    // if (zenith_deg > nearest_zenith)
    // {
    //     other_id = nearest_id + 1;
    // }
    // else
    // {
    //     other_id = nearest_id - 1;
    // }

    // *gain = gps_sat->antenna_gain[nearest_id]
    //       + (gps_sat->antenna_gain[other_id] - gps_sat->antenna_gain[nearest_id])
    //       * (zenith_deg - nearest_zenith) / ANTENNA_ELE_RESOLUTION_DEG;
    return 0;
}


// For test.
// int	main(int	argc,	char	const	*argv[]){
//     // gps_table gps_tables[MAX_SAT];
//     // ReadGpsTable(gps_tables);
//     InitGPSSatellite();
//     return 0;
// }
