#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "config.h"
#include "sbuffer.h"
#include "lib/tcpsock.h"
#include "connmgr.h"
#include "datamgr.h"
#include "sensor_db.h"

sbuffer_t *buffer;

int main(int argc, char *argv[]) {
    // Initialise common sbuffer
    sbuffer_init(&buffer);

    // Check if args are provided
    if(argc < 3) {
        printf("Please provide the right arguments: first the port, then the max nb of clients\n");
        return -1;
    }

    // Start connmgr thread
    run_arg_t conn_arg;
    conn_arg.max_conn = atoi(argv[2]);
    conn_arg.port = atoi(argv[1]);
    conn_arg.buffer = buffer;

    pthread_t connmgr_thread;
    pthread_create(&connmgr_thread, NULL, run_connmgr, &conn_arg);

    // Start datamgr thread
    datamgr_args_t datamgr_args;
    datamgr_args.fp_sensor_map = fopen("room_sensor.map", "r");
    datamgr_args.buffer = buffer;

    pthread_t datamgr_thread;
    pthread_create(&datamgr_thread, NULL, run_datamgr, &datamgr_args);

    // Start sensor_db thread
    pthread_t db_thread;
    pthread_create(&db_thread, NULL, run_db, buffer);

    // Join all threads
    pthread_join(connmgr_thread, NULL);
    pthread_join(datamgr_thread, NULL);
    pthread_join(db_thread, NULL);

    // Free common sbuffer
    sbuffer_free(&buffer);
}
