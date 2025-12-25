#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "config.h"
// #include "logger.h"
#include "sensor_db.h"
#include "sbuffer.h"

/*
static bool logger_active = false;

static void start_logger() {
    if (!logger_active) {
        if (create_log_process() == 0) {
            logger_active = true;
        } else {
            logger_active = false;
            fprintf(stderr, "[!] ERR: Could not start log process");
        }
    }
}

static void stop_logger() {
    if (logger_active) {
        end_log_process();
        logger_active = false;
    }
}
*/

void *run_db(void *arg) {
    sbuffer_t *buffer = (sbuffer_t *)arg;
    FILE * fp_csv = open_db("data.csv", false);
    if (!fp_csv) return NULL;

    sensor_data_t sd;

    while (true) {
        if (sbuffer_remove(buffer, &sd, 1) != SBUFFER_NO_DATA) {
            insert_sensor(fp_csv, sd.id, sd.value, sd.ts);
        } else {
            break;
        }
    }

    close_db(fp_csv);
    return NULL;
}

FILE *open_db(char *filename, bool append) {
    // start_logger();
    FILE *f = fopen(filename, append ? "a" : "w");

    if (!f) {
        fprintf(stderr, "[!] ERR: Could not open db");
        return NULL;
    }

    // write_to_log_process("Data file opened.");

    return f;
}

int insert_sensor(FILE *f, sensor_id_t id, sensor_value_t value, sensor_ts_t ts) {
    fprintf(f, "%" PRIu16 ", %f, %lld\n", id, value, (long long) ts);
    fflush(f);
    // write_to_log_process("Data inserted.");

    return 0;
}

int close_db(FILE *f) {
    if (!f) {
        fprintf(stderr, "[!] ERR: Tried to close NULL file");
    }

    fclose(f);
    // write_to_log_process("Data file closed.");
    // stop_logger();
    return 0;
}
