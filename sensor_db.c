#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "config.h"
#include "sensor_db.h"
#include "sbuffer.h"

/* LOGGER CODE */
static int pipe_fd[2] = {-1, -1};
static pid_t logger_pid = -1;
static int seq_num = 0;

static int create_log_process() {
    if (pipe(pipe_fd) == -1) {
        fprintf(stderr, "[!] ERR: Could not create pipe");
        return 1;
    }

    logger_pid = fork();
    if (logger_pid == -1) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        pipe_fd[0] = pipe_fd[1] = -1; // Safekeeping
        fprintf(stderr, "[!] ERR: Could not create child");
        return 1;
    }

    if (logger_pid == 0) {
        close(pipe_fd[1]);

        FILE *in = fdopen(pipe_fd[0], "r");
        if (!in) {
            fprintf(stderr, "[!] ERR: Could not read fd");
            _exit(1);
        }

        FILE *log_file = fopen("gateway.log", "w");
        if (!log_file) {
            fprintf(stderr, "[!] ERR: Could not open log file");
            fclose(in);
            _exit(1);
        }

        char line[512];
        // Main loop
        while (fgets(line, sizeof(line), in)) {
            // Can also add seqnum/timestamp in next function
            time_t timestamp = time(0);
            char *time_str = ctime(&timestamp);

            // Formatting; trimming \n
            time_str[strcspn(time_str, "\n")] = '\0';
            line[strcspn(line, "\n")] = '\0';

            fprintf(log_file, "%d - %s - %s\n", seq_num, time_str, line);
            seq_num++;

            fflush(log_file);
        }

        fclose(log_file);
        fclose(in);

        _exit(0);
    } else if (logger_pid > 0) {
        close(pipe_fd[0]);
    }

    return 0;
}

int write_to_log_process(char* msg) {
    if (pipe_fd[1] == -1) {
        return -1;
    }

    int len = strlen(msg);
    if (len != 0 || msg[len - 1] != '\n') {
        dprintf(pipe_fd[1], "%s\n", msg);
    } else {
        dprintf(pipe_fd[1], "%s", msg);
    }

    return 0;
}

static int end_log_process() {
    if (pipe_fd[1] != -1) {
        close(pipe_fd[1]);
        pipe_fd[1] = -1;
    }

    if (logger_pid > 0) {
        waitpid(logger_pid, NULL, 0);
        logger_pid = -1;
    }

    return 0;
}

bool logger_active = false;

void start_logger() {
    if (!logger_active) {
        if (create_log_process() == 0) {
            logger_active = true;
        } else {
            logger_active = false;
            fprintf(stderr, "[!] ERR: Could not start log process");
        }
    }
}

void stop_logger() {
    if (logger_active) {
        end_log_process();
        logger_active = false;
    }
}


/* STORAGE MANAGER CODE */

void *run_db(void *arg) {
    sbuffer_t *buffer = (sbuffer_t *)arg;
    FILE * fp_csv = open_db("data.csv", false);
    if (!fp_csv) return NULL;

    write_to_log_process("A new data.csv file has been created.");

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
    FILE *f = fopen(filename, append ? "a" : "w");

    if (!f) {
        fprintf(stderr, "[!] ERR: Could not open db");
        return NULL;
    }

    return f;
}

int insert_sensor(FILE *f, sensor_id_t id, sensor_value_t value, sensor_ts_t ts) {
    fprintf(f, "%" PRIu16 ", %f, %lld\n", id, value, (long long) ts);
    fflush(f);

    char log_msg[128];
    snprintf(log_msg, sizeof(log_msg),
            "Data insertion from sensor %u succeeded", id);
    write_to_log_process(log_msg);

    return 0;
}

int close_db(FILE *f) {
    if (!f) {
        fprintf(stderr, "[!] ERR: Tried to close NULL file");
    }

    write_to_log_process("The data.csv file has been closed");

    fclose(f);
    return 0;
}
