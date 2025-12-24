#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "config.h"
#include "sbuffer.h"

sbuffer_t *buffer;

int main(int argc, char *argv[]) {
    sbuffer_init(&buffer);
    sbuffer_free(&buffer);
}
