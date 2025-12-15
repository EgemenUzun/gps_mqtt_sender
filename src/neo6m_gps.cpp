#include "neo6m_gps.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ArduinoJson.h>

/**
 * @brief Converts NMEA latitude/longitude string (DDMM.LLLL) to decimal degrees.
 * @param raw_coord The raw coordinate string in DDMML.LLLL format.
 * @return Coordinate in decimal degrees format.
 */
static double convert_to_decimal_degrees(const char *raw_coord) {
    if (raw_coord == NULL || raw_coord[0] == '\0') {
        return 0.0;
    }
    
    char degrees_str[4]; // DD (for lat) or DDD (for lon) + '\0'
    char minutes_str[15]; // MM.LLLLLL + '\0'
    
    // Find the decimal point
    const char *decimal_point = strchr(raw_coord, '.');
    if (decimal_point == NULL) {
        return 0.0;
    }
    
    // Minutes (MM.LLLL...) starts 2 characters before the decimal point
    int minutes_start_idx = (decimal_point - raw_coord) - 2;
    if (minutes_start_idx < 0) {
        return 0.0; // Error
    }

    // Copy minutes and fractional part (MM.LLLL...)
    strncpy(minutes_str, raw_coord + minutes_start_idx, 2);
    minutes_str[2] = '.';
    strcpy(minutes_str + 3, decimal_point + 1);
    
    // Degrees are the characters before the minutes part
    int degrees_len = minutes_start_idx;
    if (degrees_len <= 0) {
        return 0.0;
    }
    
    strncpy(degrees_str, raw_coord, degrees_len);
    degrees_str[degrees_len] = '\0';
    
    double degrees = atof(degrees_str);
    double minutes = atof(minutes_str);
    
    return degrees + (minutes / 60.0);
}

// Checksum verification
bool nmea_check_checksum(const char *sentence) {
    // Find the starting '$' character
    const char *data_start = strstr(sentence, "$");
    if (data_start == NULL) return false;
    data_start++; // Skip '$'

    // Find the '*' character
    const char *checksum_start = strchr(data_start, '*');
    if (checksum_start == NULL) return false;

    // Calculate the checksum (XOR operation)
    uint8_t calculated_checksum = 0;
    for (const char *p = data_start; p != checksum_start; p++) {
        calculated_checksum ^= *p;
    }

    // Extract the actual checksum value (2 hex characters after '*')
    char actual_checksum_str[3] = {0};
    if (strlen(checksum_start) < 3) return false;
    
    strncpy(actual_checksum_str, checksum_start + 1, 2);
    
    // Convert the hex string to a number
    uint8_t actual_checksum = (uint8_t)strtoul(actual_checksum_str, NULL, 16);

    // Compare
    return calculated_checksum == actual_checksum;
}

// GPRMC Parsing
bool parse_gprmc(const char *sentence, NMEA_GPRMC_t *data) {
    if (!nmea_check_checksum(sentence)) {
        printf("Error: GPRMC Checksum verification failed.\n");
        return false;
    }

    // Start from '$' character to skip any prefixes
    const char *data_start = strchr(sentence, '$');
    if (data_start == NULL) return false;
    data_start++; // Skip '$'

    char temp_sentence[NMEA_MAX_LEN];
    strncpy(temp_sentence, data_start, NMEA_MAX_LEN);
    temp_sentence[NMEA_MAX_LEN - 1] = '\0';
    
    // Truncate at '*'
    char *checksum_pos = strchr(temp_sentence, '*');
    if (checksum_pos) *checksum_pos = '\0';

    char *tokens[13]; // GPRMC has about 13 fields.
    int i = 0;
    char *current = temp_sentence;
    
    // Split fields by comma (important: handling of empty fields like ",," is done here)
    while (i < 13) {
        char *comma = strchr(current, ',');
        if (comma) {
            *comma = '\0';
            tokens[i++] = current;
            current = comma + 1;
        } else {
            tokens[i++] = current;
            break;
        }
    }
    
    // Parse GPRMC fields (tokens[0] is the sentence ID 'GPRMC')
    if (i < 12) {
        printf("Error: Not enough fields in GPRMC sentence.\n");
        return false;
    }

    // Field 1: UTC Time
    data->time_utc = atof(tokens[1]);
    // Field 2: Status
    data->status = tokens[2][0];
    
    // Field 3 & 4: Latitude and Hemisphere
    data->latitude = convert_to_decimal_degrees(tokens[3]);
    data->lat_hemi = tokens[4][0];
    if (data->lat_hemi == 'S') {
        data->latitude *= -1.0;
    }
    
    // Field 5 & 6: Longitude and Hemisphere
    data->longitude = convert_to_decimal_degrees(tokens[5]);
    data->lon_hemi = tokens[6][0];
    if (data->lon_hemi == 'W') {
        data->longitude *= -1.0;
    }
    
    // Field 7: Speed over ground
    data->speed_knots = atof(tokens[7]);
    // Field 9: UTC Date (DDMMYY)
    data->date_utc = (uint32_t)atof(tokens[9]);
    
    // Remaining fields are ignored for this basic parser.

    return true;
}


// GPGGA Parsing
bool parse_gpgga(const char *sentence, NMEA_GGA_t *data) {
    if (!nmea_check_checksum(sentence)) {
        printf("Error: GPGGA Checksum verification failed.\n");
        return false;
    }

    // Start from '$' character to skip any prefixes
    const char *data_start = strchr(sentence, '$');
    if (data_start == NULL) return false;
    data_start++; // Skip '$'

    char temp_sentence[NMEA_MAX_LEN];
    strncpy(temp_sentence, data_start, NMEA_MAX_LEN);
    temp_sentence[NMEA_MAX_LEN - 1] = '\0';
    
    // Truncate at '*'
    char *checksum_pos = strchr(temp_sentence, '*');
    if (checksum_pos) *checksum_pos = '\0';

    char *tokens[16]; // GPGGA has 15 fields.
    int i = 0;
    char *current = temp_sentence;
    
    // Split fields by comma (handling empty fields)
    while (i < 16) {
        char *comma = strchr(current, ',');
        if (comma) {
            *comma = '\0';
            tokens[i++] = current;
            current = comma + 1;
        } else {
            tokens[i++] = current;
            break;
        }
    }
    
    // Parse GPGGA fields (tokens[0] is the sentence ID 'GPGGA')
    if (i < 14) {
        printf("Error: Not enough fields in GPGGA sentence.\n");
        return false;
    }

    // Field 1: UTC Time
    data->time_utc = atof(tokens[1]);
    
    // Field 2 & 3: Latitude and Hemisphere
    data->latitude = convert_to_decimal_degrees(tokens[2]);
    data->lat_hemi = tokens[3][0];
    if (data->lat_hemi == 'S') {
        data->latitude *= -1.0;
    }
    
    // Field 4 & 5: Longitude and Hemisphere
    data->longitude = convert_to_decimal_degrees(tokens[4]);
    data->lon_hemi = tokens[5][0];
    if (data->lon_hemi == 'W') {
        data->longitude *= -1.0;
    }
    
    // Field 6: Fix Quality
    data->fix_quality = atoi(tokens[6]);
    // Field 7: Number of Satellites
    data->num_satellites = atoi(tokens[7]);
    // Field 8: HDOP
    data->hdop = atof(tokens[8]);
    // Field 9 & 10: Altitude MSL and Unit
    data->altitude_msl = atof(tokens[9]);
    data->alt_unit = tokens[10][0];
    // Field 11 & 12: Geoid Separation and Unit
    data->geoid_sep = atof(tokens[11]);
    data->geo_sep_unit = tokens[12][0];
    
    // Remaining fields are ignored for this basic parser.

    return true;
}

JsonDocument serialize_gprmc(const NMEA_GPRMC_t *data) {
    JsonDocument doc;
    doc["id"]           = "06ABC123";
    doc["time_utc"]     = data->time_utc;
    doc["status"]       = String(data->status);
    doc["latitude"]     = data->latitude;
    doc["lat_hemi"]     = String(data->lat_hemi);
    doc["longitude"]    = data->longitude;
    doc["lon_hemi"]     = String(data->lon_hemi);
    doc["speed_knots"]  = data->speed_knots;
    doc["track_angle"]  = data->track_angle;
    doc["date_utc"]     = data->date_utc;
    return doc;
}

JsonDocument serialize_gpgga(const NMEA_GGA_t *data) {
    JsonDocument doc;
    doc["id"]             = "06ABC123";
    doc["time_utc"]       = data->time_utc;
    doc["latitude"]       = data->latitude;
    doc["lat_hemi"]       = String(data->lat_hemi);
    doc["longitude"]      = data->longitude;
    doc["lon_hemi"]       = String(data->lon_hemi);
    doc["fix_quality"]    = data->fix_quality;
    doc["num_satellites"] = data->num_satellites;
    doc["hdop"]           = data->hdop;
    doc["altitude_msl"]   = data->altitude_msl;
    doc["alt_unit"]       = String(data->alt_unit);
    doc["geoid_sep"]      = data->geoid_sep;
    doc["geo_sep_unit"]   = String(data->geo_sep_unit);
    return doc;
}

JsonDocument serialize_gps(const NMEA_GGA_t *gga, const NMEA_GPRMC_t *rmc) {
    JsonDocument doc;
    doc["id"]             = "06ABC123";
    doc["latitude"]       = gga->latitude;
    doc["lat_hemi"]       = String(gga->lat_hemi);
    doc["longitude"]      = gga->longitude;
    doc["lon_hemi"]       = String(gga->lon_hemi);
    doc["num_satellites"] = gga->num_satellites;
    doc["altitude_msl"]   = gga->altitude_msl;
    doc["alt_unit"]       = String(gga->alt_unit);
    doc["speed_knots"]    = rmc->speed_knots;
    doc["track_angle"]    = rmc->track_angle;
    doc["status"]         = String(rmc->status);
    doc["date_utc"]       = rmc->date_utc;
    doc["time_utc"]       = rmc->time_utc;
    doc["timestamp"]      = 0;
    doc["route_id"]       = 1;
    return doc;
}

void send_payload(HardwareSerial &serial, const uint8_t *payload, size_t len) {
    serial.write(payload, len);
}

void configure_neo6m(HardwareSerial &serial) {
  send_payload(serial, setRate5Hz, sizeof(setRate5Hz));
  send_payload(serial, disableGLL, sizeof(disableGLL));
  send_payload(serial, disableGSA, sizeof(disableGSA));
  send_payload(serial, disableGSV, sizeof(disableGSV));
  send_payload(serial, disableVTG, sizeof(disableVTG));
  send_payload(serial, enableGGA, sizeof(enableGGA));
  send_payload(serial, enableRMC, sizeof(enableRMC));
  send_payload(serial, saveConfig, sizeof(saveConfig));
}