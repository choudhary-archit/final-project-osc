#include "lib/tcpsock.h"
#include "sbuffer.h"

// Arguments to run / handler
typedef struct {
    int max_conn;
    int port;
    sbuffer_t *buffer;
} conn_args_t;

typedef struct {
    tcpsock_t *client;
    sbuffer_t *buffer;
} node_handler_args_t;

// Handle clients
void *node_handler(void *arg);

// Run connection manager
void *run_connmgr(void *arg);
