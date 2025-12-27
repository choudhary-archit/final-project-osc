#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#include "lib/dplist.h"
#include "datamgr.h"
#include "config.h"
#include "sensor_db.h"


// Defining element of dplist
typedef uint16_t room_id_t;

typedef struct node_info {
    sensor_id_t sensor_id;
    room_id_t room_id;
    sensor_value_t *prev_vals;
    sensor_value_t running_avg;
    sensor_ts_t last_modified;
} node_info_t;

// Element copy, free, compare for dplist
void *element_copy(void *element) {
    node_info_t *orig = (node_info_t *)element;
    node_info_t *copy = malloc(sizeof(*copy));

    copy->sensor_id = orig->sensor_id;
    copy->room_id = orig->room_id;
    copy->running_avg = orig->running_avg;
    copy->last_modified = orig->last_modified;

    // Deep copy of prev_vals
    copy->prev_vals = NULL;
    if (orig->prev_vals) {
        copy->prev_vals = malloc(sizeof(sensor_value_t) * RUN_AVG_LENGTH);
        if (!copy->prev_vals) { free(copy); return NULL; }
        memcpy(copy->prev_vals, orig->prev_vals,
                sizeof(sensor_value_t) * RUN_AVG_LENGTH);
    }

    return (void *)copy;
}

void element_free(void **element) {
    if (!element || !*element) return;

    node_info_t *e = (node_info_t *)(*element);
    free(e->prev_vals);
    e->prev_vals = NULL;

    free(e);
    *element = NULL;
}

int element_compare(void *x, void *y) {
    node_info_t *a = (node_info_t *)x;
    node_info_t *b = (node_info_t *)y;

    if (a->sensor_id < b->sensor_id) return -1;
    if (a->sensor_id > b->sensor_id) return 1;
    return 0;
}

// Create globally accessible dplist
dplist_t *sensor_info;

// Helper function
node_info_t *get_element_from_id(sensor_id_t id) {
    node_info_t dummy_element;
    dummy_element.sensor_id = id;

    // Element -> Index
    int index = dpl_get_index_of_element(sensor_info, &dummy_element);
    if (index < 0) return NULL;

    // Index -> Reference (to node)
    dplist_node_t *ref = dpl_get_reference_at_index(sensor_info, index);
    if (!ref) return NULL;

    // Reference -> Element
    node_info_t *element = dpl_get_element_at_reference(sensor_info, ref);

    return element;
}

// Parse sensor data and arrange it in a dplist
void *run_datamgr(void *args) {
    // Parse args
    datamgr_args_t* datamgr_args = (datamgr_args_t *)args;
    FILE * fp_sensor_map = datamgr_args->fp_sensor_map;
    sbuffer_t *buffer = datamgr_args->buffer;

    // Initialise dplist
    sensor_info = dpl_create(element_copy, element_free, element_compare);

    // Read sensor map file
    int sensor_id, room_id;
    while (fscanf(fp_sensor_map, "%d %d", &room_id, &sensor_id) == 2) {
        node_info_t node;

        // Set id information
        node.sensor_id = sensor_id;
        node.room_id = room_id;

        // Initialise running average machinery
        node.prev_vals = (sensor_value_t *)malloc(
                sizeof(sensor_value_t) * RUN_AVG_LENGTH);
        for (int i = 0; i < RUN_AVG_LENGTH; i++) node.prev_vals[i] = -99999;
        node.running_avg = 0;

        // Insert node into dplist
        dpl_insert_at_index(sensor_info, &node, 0, true);
    }

    // Read sensor data from sbuffer
    sensor_data_t sd;

    while (true) {
        // Take latest reading from sbuffer
        if (sbuffer_remove(buffer, &sd, 0) != SBUFFER_NO_DATA) {
            // Find element in dplist
            node_info_t *element = get_element_from_id(sd.id);

            // Update timestamp
            element->last_modified = sd.ts;

            // Update prev_vals
            for (int i = 0; i < RUN_AVG_LENGTH - 1; i++) {
                (element->prev_vals)[i] = (element->prev_vals)[i+1];
            }
            (element->prev_vals)[RUN_AVG_LENGTH - 1] = sd.value;

            // Update running_avg
            element->running_avg = 0;
            for (int i = 0; i < RUN_AVG_LENGTH; i++) {
                element->running_avg += (element->prev_vals)[i] / RUN_AVG_LENGTH;
            }

            char log_msg[128];
            // Check if value outside bounds
            if ((element->prev_vals)[0] != -99999) {
                if (element->running_avg < SET_MIN_TEMP) {
                    snprintf(log_msg, sizeof(log_msg),
                            "Sensor node %u reports it's too cold (avg temp = %f)",
                            element->sensor_id, element->running_avg);
                    write_to_log_process(log_msg);
                } else if (element->running_avg > SET_MAX_TEMP) {
                    snprintf(log_msg, sizeof(log_msg),
                            "Sensor node %u reports it's too hot (avg temp = %f)",
                            element->sensor_id, element->running_avg);
                    write_to_log_process(log_msg);
                }
            }
        } else {
            break;
        }
    }

    return NULL;
}

// Free sensor data
void datamgr_free(dplist_t **sensor_info) {
    dpl_free(sensor_info, true);
}

uint16_t datamgr_get_room_id(sensor_id_t sensor_id) {
    node_info_t *element = get_element_from_id(sensor_id);
    return (uint16_t) element->room_id;
}

sensor_value_t datamgr_get_avg(sensor_id_t sensor_id) {
    node_info_t *element = get_element_from_id(sensor_id);
    return element->running_avg;
}

time_t datamgr_get_last_modified(sensor_id_t sensor_id) {
    node_info_t *element = get_element_from_id(sensor_id);
    return (time_t) element->last_modified;
}

int datamgr_get_total_sensors() {
    return dpl_size(sensor_info);
}
