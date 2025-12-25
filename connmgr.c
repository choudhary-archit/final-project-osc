/**
 * \author Archit Choudhary
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <pthread.h>
#include "connmgr.h"
#include "sbuffer.h"
#include "config.h"
#include "lib/tcpsock.h"

pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

void *node_handler(void *arg) {
    handler_arg_t *handler_arg = (handler_arg_t *)arg;
    tcpsock_t *client = handler_arg->client;
    sbuffer_t *buffer = handler_arg->buffer;

    int bytes, result;
    int result1, result2, result3;
    sensor_data_t data;

    do {
        // read sensor ID
        bytes = sizeof(data.id);
        result1 = tcp_receive(client, (void *) &data.id, &bytes);

        // read temperature
        bytes = sizeof(data.value);
        result2 = tcp_receive(client, (void *) &data.value, &bytes);

        // read timestamp
        bytes = sizeof(data.ts);
        result3 = tcp_receive(client, (void *) &data.ts, &bytes);

        // 1 iff all are 1
        result = result1 * result2 * result3;

        if ((result == TCP_NO_ERROR) && bytes)
            sbuffer_insert(buffer, &data);
    } while (result == TCP_NO_ERROR);

    // Check if client sends close signal (instead of no error signal)
    if (result == TCP_CONNECTION_CLOSED)
        printf("Peer has closed connection\n");
    else
        printf("Error occured on connection to peer\n");

    // Close client
    tcp_close(&client);
    return NULL;
}

void *run_connmgr(void *arg) {
    run_arg_t *conn_arg = (run_arg_t *)arg;
    int max_conn = conn_arg->max_conn;
    int port = conn_arg->port;
    sbuffer_t *buffer = conn_arg->buffer;

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

        handler_arg_t handler_args;
        handler_args.client = client;
        handler_args.buffer = buffer;

        pthread_create(&thread_id, NULL, node_handler, &handler_args);
        conn_counter++;

        // Detach each client thread
        pthread_detach(thread_id);
    } while (conn_counter < max_conn);

    // Closing server
    if (tcp_close(&server) != TCP_NO_ERROR) exit(EXIT_FAILURE);
    printf("Test server is shutting down\n");
}
