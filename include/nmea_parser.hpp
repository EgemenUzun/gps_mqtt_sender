#ifndef NMEA_PARSER_HPP
#define NMEA_PARSER_HPP

#include <stdint.h>
#include <stdbool.h>

#define NMEA_MAX_LEN 100

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double time_utc;
    char status;            // 'A' / 'V'
    double latitude;
    char lat_hemi;          // 'N' / 'S'
    double longitude;
    char lon_hemi;          // 'E' / 'W'
    double speed_knots;
    double track_angle;
    uint32_t date_utc;
} NMEA_GPRMC_t;

typedef struct {
    double time_utc;
    double latitude;
    char lat_hemi;
    double longitude;
    char lon_hemi;
    int fix_quality;
    int num_satellites;
    double hdop;
    double altitude_msl;
    char alt_unit;
    double geoid_sep;
    char geo_sep_unit;
} NMEA_GGA_t;

// C-compatible functions
bool nmea_check_checksum(const char *sentence);
bool parse_gprmc(const char *sentence, NMEA_GPRMC_t *data);
bool parse_gpgga(const char *sentence, NMEA_GGA_t *data);

#ifdef __cplusplus
}
#endif


// -------- C++ Only Serialization Section --------
#ifdef __cplusplus

#include <ArduinoJson.h>

// Return by value → SAFE (no dynamic allocation)
JsonDocument serialize_gprmc(const NMEA_GPRMC_t *data);
JsonDocument serialize_gpgga(const NMEA_GGA_t *data);

#endif // __cplusplus

#endif // NMEA_PARSER_HPP
