// Adafruit Ultimate GPS

#ifdef UGPS

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "boards.h"
#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_drv_twi.h"
#include "app_scheduler.h"
#include "app_util_platform.h"
#include "app_twi.h"
#include "twi.h"
#include "gpio.h"
#include "config.h"
#include "timer.h"
#include "sensor.h"
#include "storage.h"
#include "comm.h"
#include "misc.h"
#include "ugps.h"
#include "io.h"

// Serial I/O Buffers
#define IOBUFFERS 8
#define MAXLINE 250

typedef struct {
    uint16_t linesize;
    char linebuf[MAXLINE];
} iobuf_t;
static iobuf_t iobuf[IOBUFFERS];
static uint16_t iobuf_completed;
static uint16_t iobuf_filling;
static int completed_iobufs_available;

static float reported_latitude = 0.0;
static float reported_longitude = 0.0;
static float reported_altitude = 0.0;
static uint32_t reported_date;
static uint32_t reported_time;
static bool reported = false;
static bool reported_have_location;
static bool reported_have_full_location;
static bool reported_have_timedate;

static bool initialized = false;
static bool shutdown = false;
static uint32_t sentences_received = 0;
static uint32_t seconds = 0;
static bool skip = false;

// Update net iteration
void s_ugps_update(void) {
    skip = false;
}

// Init sensor just after each power-on
void s_ugps_done_settling() {

    // Clear out the values
    s_ugps_clear_measurement();

}

// Force the GPS to shutdown
void s_ugps_shutdown() {
    if (!shutdown && gpio_current_uart() == UART_GPS) {
        shutdown = true;
        gpio_indicator_no_longer_needed(GPS);
        comm_repeat_initial_select();
    }

}

// Term sensor just before each power-off
bool s_ugps_term() {
    if (!initialized)
        return false;
    initialized = false;
    return true;
}

// One-time initialization of sensor
bool s_ugps_init() {
    if (initialized)
        return false;
    completed_iobufs_available = 0;
    sentences_received = 0;
    iobuf_filling = 0;
    iobuf[0].linesize = 0;
    initialized = true;
    seconds = 0;
    return true;
}

// Reset the buffer
void iobuf_reset() {
    iobuf[iobuf_filling].linesize = 0;
}

// Drain the completed I/O buffer and point to the next
void iobuf_pop() {
    if (completed_iobufs_available > 0) {
        if (++iobuf_completed >= IOBUFFERS)
            iobuf_completed = 0;
        completed_iobufs_available--;
    }
}

// Process a data-received events
void line_event_handler(void *p_event_data, uint16_t event_size) {
    uint16_t completed_iobuf = * (uint16_t *) p_event_data;
    char *line = iobuf[completed_iobuf].linebuf;
    uint16_t linelen = strlen(line);

    // Bump the number received
    sentences_received = 0;

    // Process the GPS sentence
    if (debug(DBG_GPS_MAX))
        DEBUG_PRINTF("%s%s%s %s\n", reported_have_location ? "l" : "-", reported_have_full_location ? "L" : "-", reported_have_timedate ? "T" : "-", line);

    // Process $GPGGA, which should give us lat/lon/alt
    if (memcmp(line, "$GPGGA", 6) == 0) {
        int j;
        char *lat, *ns, *lon, *ew, *alt;
        bool haveLat, haveLon, haveNS, haveEW, haveAlt;
        uint16_t commas;

        commas = 0;
        haveLat = haveLon = haveNS = haveEW = haveAlt = false;
        lat = ns = lon = ew = alt = "";

        for (j = 0; j < linelen; j++)
            if (line[j] == ',') {
                line[j] = '\0';
                commas++;
                if (commas == 1) {
                    // Sitting at comma before Time
                }
                if (commas == 2) {
                    // Sitting at comma before Lat
                    lat = (char *) &line[j + 1];
                }
                if (commas == 3) {
                    // Sitting at comma before NS
                    haveLat = (lat[0] != '\0');
                    ns = (char *) &line[j + 1];
                }
                if (commas == 4) {
                    // Sitting at comma before Lon
                    haveNS = (ns[0] != '\0');
                    lon = (char *) &line[j + 1];
                }
                if (commas == 5) {
                    // Sitting at comma before EW
                    haveLon = (lon[0] != '\0');
                    ew = (char *) &line[j + 1];
                }
                if (commas == 6) {
                    // Sitting at comma before Quality
                    haveEW = (ew[0] != '\0');
                }
                if (commas == 7) {
                    // Sitting at comma before NumSat
                }
                if (commas == 8) {
                    // Sitting at comma before Hdop
                }
                if (commas == 9) {
                    // Sitting at comma before Alt
                    alt = (char *) &line[j + 1];
                }
                if (commas == 10) {
                    // Sitting at comma before M unit
                    haveAlt = (alt[0] != '\0');
                    break;
                }
            }

        // If we've got what we need, process it and exit.
        if (haveLat && haveNS & haveLon && haveEW) {
            float fLatitude = GpsEncodingToDegrees(lat, ns);
            float fLongitude = GpsEncodingToDegrees(lon, ew);
            if (fLatitude != 0 && fLongitude != 0) {
                reported_have_location = true;
                reported_latitude = fLatitude;
                reported_longitude = fLongitude;
                if (haveAlt) {
                    reported_altitude = atof(alt);
                    reported_have_full_location = true;
                }
            }

        }

    }   // if GPGGA

    // Process $GPRMC, which should give us lat/lon and time
    if (memcmp(line, "$GPRMC", 6) == 0) {
        int j;
        char *lat, *ns, *lon, *ew, *time, *date;
        bool haveLat, haveLon, haveNS, haveEW, haveTime, haveDate;
        uint16_t commas;

        commas = 0;
        haveLat = haveLon = haveNS = haveEW = haveTime = haveDate = false;
        lat = ns = lon = ew = date = time = "";

        for (j = 0; j < linelen; j++)
            if (line[j] == ',') {
                line[j] = '\0';
                commas++;
                if (commas == 1) {
                    // Sitting at comma before Time
                    time = (char *) &line[j + 1];
                }
                if (commas == 2) {
                    haveTime = (time[0] != '\0');
                    // Sitting at comma before Validity
                }
                if (commas == 3) {
                    // Sitting at comma before Lat
                    lat = (char *) &line[j + 1];
                }
                if (commas == 4) {
                    // Sitting at comma before NS
                    haveLat = (lat[0] != '\0');
                    ns = (char *) &line[j + 1];
                }
                if (commas == 5) {
                    // Sitting at comma before Lon
                    haveNS = (ns[0] != '\0');
                    lon = (char *) &line[j + 1];
                }
                if (commas == 6) {
                    // Sitting at comma before EW
                    haveLon = (lon[0] != '\0');
                    ew = (char *) &line[j + 1];
                }
                if (commas == 7) {
                    // Sitting at comma before Speed
                    haveEW = (ew[0] != '\0');
                }
                if (commas == 8) {
                    // Sitting at comma before True Course
                }
                if (commas == 9) {
                    // Sitting at comma before Date
                    date = (char *) &line[j + 1];
                }
                if (commas == 10) {
                    // Sitting at comma before Variation
                    haveDate = (date[0] != '\0');
                    break;
                }
            }

        // If we've got lat/lon, process it
        if (haveLat && haveNS & haveLon && haveEW) {
            float fLatitude = GpsEncodingToDegrees(lat, ns);
            float fLongitude = GpsEncodingToDegrees(lon, ew);
            if (fLatitude != 0 && fLongitude != 0) {
                reported_latitude = fLatitude;
                reported_longitude = fLongitude;
                reported_have_location = true;
            }

        }

        // If we've got what we need, process it and exit.
        if (haveTime && haveDate) {
            reported_time = atol(time);
            reported_date = atol(date);
            set_timestamp(reported_date, reported_time);
            reported_have_timedate = true;
        }

    }   // if GPRMC

    // Release this buffer, which can now be re-used
    iobuf_pop();

}

// Bump to the next I/O buffer, dropping the line if we overflow I/O buffers
bool iobuf_push() {
    bool dropped = false;

    if (completed_iobufs_available < IOBUFFERS) {
        completed_iobufs_available++;
        if (++iobuf_filling >= IOBUFFERS)
            iobuf_filling = 0;
        if (app_sched_event_put(&iobuf_completed, sizeof(iobuf_completed), line_event_handler) != NRF_SUCCESS)
            DEBUG_PRINTF("UGPS sched put error\n");
    } else {
        dropped = true;
        DEBUG_PRINTF("UGPS RX OVERRUN\n");
    }

    // Initialize the buffer
    iobuf_reset();

    return !dropped;
}

// Process byte received from gps
void s_ugps_received_byte(uint8_t databyte) {

    // Discard results if we happen to get here if not yet initialized
    if (!initialized)
        return;

    // If we get here, we're expecting to receive an ASCII text command terminated in \r\n.
    // If this is the CR of the CRLF sequence, skip it.
    if (databyte == '\r')
        return;

    // If this the final byte of the CRLF sequence, process it
    if (databyte == '\n') {

        // If it's just a blank line (or the second char of \r\n), skip it
        if (iobuf[iobuf_filling].linesize == 0)
            return;

        // Null terminate the line because it's a string
        iobuf[iobuf_filling].linebuf[iobuf[iobuf_filling].linesize] = '\0';

        // Enqueue it
        iobuf_push();

        return;
    }

    // If somehow a non-text character got here, substitute.
    if (databyte < 0x20 || databyte > 0x7f)
        databyte = '.';

    // Add the char to the line buffer
    if (iobuf[iobuf_filling].linesize < (sizeof(iobuf[iobuf_filling].linebuf)-2))
        iobuf[iobuf_filling].linebuf[iobuf[iobuf_filling].linesize++] = (char) databyte;
    else
        DEBUG_PRINTF("UGPS line overrun\n");

}

// Clear the values
void s_ugps_clear_measurement() {
    reported = false;
    reported_have_location = false;
    reported_have_full_location = false;
    reported_have_timedate = false;
}

// Group skip handler
bool s_ugps_skip(void *g) {
    uint16_t gps_status = comm_gps_get_value(NULL, NULL, NULL);
    if (skip)
        return true;
    return (gps_status == GPS_LOCATION_FULL || gps_status == GPS_LOCATION_PARTIAL);
}

// Poller
void s_ugps_poll(void *g) {

    // Exit if we're not supposed to be here
    if (!sensor_is_polling_valid(g))
        return;

    // Keep track of how long we've been waiting for lock
    seconds += GPS_POLL_SECONDS;

    // Determine whether or not it's time to stop polling
    if (reported_have_location)
        if ((seconds > ((GPS_ABORT_MINUTES-1)*60)) || (reported_have_full_location && reported_have_timedate)) {
            reported = true;
            skip = true;
            DEBUG_PRINTF("%.3f/%.3f/%.3f %lu:%lu\n", reported_latitude, reported_longitude, reported_altitude, reported_date, reported_time);
        }

    // If we've already got the full location, terminate the polling just to save battery life
    if ((comm_gps_get_value(NULL, NULL, NULL) == GPS_LOCATION_FULL) || shutdown) {
        if (sensor_group_completed(g))
            DEBUG_PRINTF("GPS acquired.\n");
        return;
    }

    // If the GPS hardware isn't even present, terminate the polling to save battery life.
    if (seconds > (GPS_ABORT_MINUTES*60)) {
        if (sensor_group_completed(g)) {
            if (s_ugps_get_value(NULL, NULL, NULL) == GPS_NO_DATA)
                DEBUG_PRINTF("GPS shutdown. (no data)\n");
            else
                DEBUG_PRINTF("GPS shutdown. (couldn't lock)\n");
        }
        return;
    }

    // Make sure it appears that we are connecting to GPS
    gpio_indicate(INDICATE_GPS_CONNECTING);

}

// Get the value
uint16_t s_ugps_get_value(float *lat, float *lon, float *alt) {
    if (!reported)
        return (sentences_received < 5 ? GPS_NO_DATA : GPS_NO_LOCATION);
    if (lat != NULL)
        *lat = reported_latitude;
    if (lon != NULL)
        *lon = reported_longitude;
    if (alt != NULL)
        *alt = reported_altitude;
    return (reported_have_full_location ? GPS_LOCATION_FULL : GPS_LOCATION_PARTIAL);
}

#endif // UGPS