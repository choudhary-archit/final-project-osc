/**
 * \author Archit Choudhary
 */

#include <sys/select.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <pthread.h>

#include "connmgr.h"
#include "sbuffer.h"
#include "config.h"
#include "sensor_db.h"
#include "lib/tcpsock.h"

pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

int wait_for_data(tcpsock_t *client) {
    fd_set rfds;
    FD_ZERO(&rfds);

    int sd;
    if (tcp_get_sd(client, &sd) != TCP_NO_ERROR) return -1;
    FD_SET(sd, &rfds);

    struct timeval tv;
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 0;

    int r = select(sd + 1, &rfds, NULL, NULL, &tv);

    // Timeout
    if (r == 0) return 0;
    // Select error
    if (r < 0) return -1;
    // It's fine, read socket
    return 1;
}

void *node_handler(void *arg) {
    node_handler_args_t *handler_args = (node_handler_args_t *)arg;
    tcpsock_t *client = handler_args->client;
    sbuffer_t *buffer = handler_args->buffer;

    free(handler_args);

    int bytes;
    sensor_data_t data;
    sensor_id_t id;

    bool logged = false;
    bool saved_id = false;

    while (1) {
        int w = wait_for_data(client);

        // Timeout
        if (w == 0) {
            printf("Client timed out.\n");
            break;
        }

        if (w < 0) {
            perror("select error");
            break;
        }

        // read sensor ID
        bytes = sizeof(data.id);
        if (tcp_receive(client, &data.id, &bytes) != TCP_NO_ERROR) break;

        if (!logged) {
            char log_msg[128];
            snprintf(log_msg, sizeof(log_msg),
                    "Sensor node %u has opened a new connection", data.id);
            write_to_log_process(log_msg);
            logged = true;
        }

        if (!saved_id) {
            id = data.id;
            saved_id = true;
        }

        // read temperature
        bytes = sizeof(data.value);
        if (tcp_receive(client, &data.value, &bytes) != TCP_NO_ERROR) break;

        // read timestamp
        bytes = sizeof(data.ts);
        if (tcp_receive(client, &data.ts, &bytes) != TCP_NO_ERROR) break;

        sbuffer_insert(buffer, &data);
    }

    // Close client
    char log_msg[128];
    snprintf(log_msg, sizeof(log_msg),
            "Sensor node %u has closed the connection", id);
    write_to_log_process(log_msg);

    tcp_close(&client);
    return NULL;
}

void *run_connmgr(void *arg) {
    conn_args_t *conn_args = (conn_args_t *)arg;
    int max_conn = conn_args->max_conn;
    int port = conn_args->port;
    sbuffer_t *buffer = conn_args->buffer;

    tcpsock_t *server, *client;

    // Open port PORT on server
    if (tcp_passive_open(&server, port) != TCP_NO_ERROR) exit(EXIT_FAILURE);

    // Client threadpool
    pthread_t *cl_threads = malloc(sizeof(*cl_threads) * max_conn);

    int conn_counter;
    for (conn_counter = 0; conn_counter < max_conn; conn_counter++) {
        // Program stops here until a client connects
        if (tcp_wait_for_connection(server, &client) != TCP_NO_ERROR)
            exit(EXIT_FAILURE);

        // Set up args for node handler
        node_handler_args_t *handler_args = malloc(sizeof(*handler_args));
        handler_args->client = client;
        handler_args->buffer = buffer;

        // Create client thread
        pthread_create(&cl_threads[conn_counter], NULL, node_handler, handler_args);
    }

    // Close server
    if (tcp_close(&server) != TCP_NO_ERROR) exit(EXIT_FAILURE);

    // Join client threads
    for (int i = 0; i < max_conn; i++) {
        pthread_join(cl_threads[i], NULL);
    }
    free(cl_threads);

    // Add EOS entry to buffer
    sensor_data_t eos = {.id = 0, .value = 0, .ts = 0};
    sbuffer_insert(buffer, &eos);

    return NULL;
}
