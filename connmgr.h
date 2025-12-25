#include "lib/tcpsock.h"
#include "sbuffer.h"

// Arguments to run / handler
typedef struct run_arg {
    int max_conn;
    int port;
    sbuffer_t *buffer;
} run_arg_t;

typedef struct handler_arg {
    tcpsock_t *client;
    sbuffer_t *buffer;
} handler_arg_t;

// Handle clients
void *node_handler(void *arg);

// Run connection manager
void *run_connmgr(void *arg);
