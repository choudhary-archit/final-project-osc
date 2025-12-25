/**
 * \author Bert Lagaisse
 * \author Archit Choudhary
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>

#include "sbuffer.h"

/**
 * basic node for the buffer, these nodes are linked together to create the buffer
 */
typedef struct sbuffer_node {
    struct sbuffer_node *next;  /**< a pointer to the next node*/
    sensor_data_t data;         /**< a structure containing the data */
    bool datamgr_read;
    bool db_read;
} sbuffer_node_t;

/**
 * a structure to keep track of the buffer
 */
struct sbuffer {
    sbuffer_node_t *head;       /**< a pointer to the first node in the buffer */
    sbuffer_node_t *tail;       /**< a pointer to the last node in the buffer */
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    int eos;
};

int sbuffer_init(sbuffer_t **buffer) {
    *buffer = malloc(sizeof(sbuffer_t));
    if (*buffer == NULL) return SBUFFER_FAILURE;
    (*buffer)->head = NULL;
    (*buffer)->tail = NULL;
    (*buffer)->eos = 0;

    pthread_mutex_init(&(*buffer)->mutex, NULL);
    pthread_cond_init(&(*buffer)->not_empty, NULL);
    return SBUFFER_SUCCESS;
}

int sbuffer_free(sbuffer_t **buffer) {
    sbuffer_node_t *dummy;
    if ((buffer == NULL) || (*buffer == NULL)) {
        return SBUFFER_FAILURE;
    }

    pthread_mutex_lock(&(*buffer)->mutex);
    while ((*buffer)->head) {
        dummy = (*buffer)->head;
        (*buffer)->head = (*buffer)->head->next;
        free(dummy);
    }
    pthread_mutex_unlock(&(*buffer)->mutex);

    pthread_mutex_destroy(&(*buffer)->mutex);
    pthread_cond_destroy(&(*buffer)->not_empty);

    free(*buffer);
    *buffer = NULL;
    return SBUFFER_SUCCESS;
}

// Consumer id is used to identify which thread is calling this
int sbuffer_remove(sbuffer_t *buffer, sensor_data_t *data, int consumer_id) {
    if (buffer == NULL) return SBUFFER_FAILURE;
    if (consumer_id != 0 && consumer_id != 1) return SBUFFER_FAILURE;

    // Lock so each thread acts in order
    pthread_mutex_lock(&buffer->mutex);

    while (true) {
        // If empty and not EOS, wait until not empty
        while (buffer->head == NULL && !buffer->eos) {
            pthread_cond_wait(&buffer->not_empty, &buffer->mutex);
        }

        // If empty and EOS, exit
        if (buffer->head == NULL && buffer->eos) {
            pthread_mutex_unlock(&buffer->mutex);
            return SBUFFER_NO_DATA;
        }

        sbuffer_node_t *dummy = buffer->head;
        *data = dummy->data;

        // Check if current thread has already read the HEAD
        bool already_read =
            (consumer_id == 0 && dummy->datamgr_read) ||
            (consumer_id == 1 && dummy->db_read);

        if (already_read) {
            pthread_cond_wait(&buffer->not_empty, &buffer->mutex);
            continue;
        }

        // Mark read field
        if (consumer_id == 0) {
            dummy->datamgr_read = true;
        } else if (consumer_id == 1) {
            dummy->db_read = true;
        }

        // Remove node if both threads have read it
        if (dummy->datamgr_read && dummy->db_read) {
            buffer->head = dummy->next;
            if (buffer->head == NULL) buffer->tail == NULL;
            free(dummy);
        }

        pthread_cond_broadcast(&buffer->not_empty);

        // If sensor id = 0, we have reached EOS
        if (data->id == 0) {
            buffer->eos = 1;
            pthread_mutex_unlock(&buffer->mutex);
            return SBUFFER_NO_DATA;
        }

        pthread_mutex_unlock(&buffer->mutex);
        return SBUFFER_SUCCESS;
    }
}


int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data) {
    sbuffer_node_t *dummy;
    if (buffer == NULL) return SBUFFER_FAILURE;

    dummy = malloc(sizeof(sbuffer_node_t));
    if (dummy == NULL) return SBUFFER_FAILURE;

    dummy->data = *data;
    dummy->next = NULL;
    dummy->datamgr_read = false;
    dummy->db_read = false;

    pthread_mutex_lock(&buffer->mutex);

    if (buffer->tail == NULL) // buffer empty (buffer->head should also be NULL
    {
        buffer->head = buffer->tail = dummy;
    } else // buffer not empty
    {
        buffer->tail->next = dummy;
        buffer->tail = buffer->tail->next;
    }

    pthread_cond_signal(&buffer->not_empty);
    pthread_mutex_unlock(&buffer->mutex);

    return SBUFFER_SUCCESS;
}
