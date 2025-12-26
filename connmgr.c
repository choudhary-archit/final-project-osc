/**
 * \author Archit Choudhary
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <inttypes.h>
#include <pthread.h>

#include "connmgr.h"
#include "sbuffer.h"
#include "config.h"
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

    int bytes, result;
    sensor_data_t data;

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

        // read temperature
        bytes = sizeof(data.value);
        if (tcp_receive(client, &data.value, &bytes) != TCP_NO_ERROR) break;

        // read timestamp
        bytes = sizeof(data.ts);
        if (tcp_receive(client, &data.ts, &bytes) != TCP_NO_ERROR) break;

        sbuffer_insert(buffer, &data);
    }

    // Close client
    tcp_close(&client);
    return NULL;
}

void *run_connmgr(void *arg) {
    conn_args_t *conn_args = (conn_args_t *)arg;
    int max_conn = conn_args->max_conn;
    int port = conn_args->port;
    sbuffer_t *buffer = conn_args->buffer;

    tcpsock_t *server, *client;
    int conn_counter = 0;

    // Open port PORT on server
    printf("Test server is started\n");
    if (tcp_passive_open(&server, port) != TCP_NO_ERROR) exit(EXIT_FAILURE);

    // Handle clients
    pthread_t thread_id;
    do {
        // Program stops here until a client connects
        if (tcp_wait_for_connection(server, &client) != TCP_NO_ERROR)
            exit(EXIT_FAILURE);
        printf("Incoming client connection\n");

        node_handler_args_t *handler_args = malloc(sizeof(*handler_args));
        handler_args->client = client;
        handler_args->buffer = buffer;

        pthread_create(&thread_id, NULL, node_handler, handler_args);
        conn_counter++;

        // Detach each client thread
        pthread_detach(thread_id);
    } while (conn_counter < max_conn);

    // Closing server
    if (tcp_close(&server) != TCP_NO_ERROR) exit(EXIT_FAILURE);
    printf("Test server is shutting down\n");
}
