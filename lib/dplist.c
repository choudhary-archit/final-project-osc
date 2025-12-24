#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "dplist.h"

/*
 * The real definition of struct list / struct node
 */

struct dplist_node {
    dplist_node_t *prev, *next;
    void *element;
};

struct dplist {
    dplist_node_t *head;

    void *(*element_copy)(void *src_element);

    void (*element_free)(void **element);

    int (*element_compare)(void *x, void *y);
};


dplist_t *dpl_create(// callback functions
        void *(*element_copy)(void *src_element),
        void (*element_free)(void **element),
        int (*element_compare)(void *x, void *y)
) {
    dplist_t *list;
    list = malloc(sizeof(struct dplist));
    list->head = NULL;
    list->element_copy = element_copy;
    list->element_free = element_free;
    list->element_compare = element_compare;
    return list;
}

void dpl_free(dplist_t **list, bool free_element) {
    if (list == NULL || *list == NULL) return;

    dplist_node_t *current_node = (*list)->head;
    dplist_node_t *next_node;

    // loop through nodes
    while (current_node != NULL) {
        next_node = current_node->next;

        // first free element (if applicable)
        if (free_element && (*list)->element_free != NULL) {
            (*list)->element_free(&(current_node->element));
        }

        // then free node
        free(current_node);
        current_node = next_node;
    }

    free(*list);
    *list = NULL;
}

dplist_t *dpl_insert_at_index(dplist_t *list, void *element, int index, bool insert_copy) {
    if (list == NULL) return NULL;

    dplist_node_t *new_node = malloc(sizeof(dplist_node_t));

    // Deep copy vs regular copy
    if (insert_copy && list->element_copy != NULL) {
        new_node->element = list->element_copy(element);
    } else {
        new_node->element = element;
    }

    // Same insertion logic as the exercises
    if (list->head == NULL) { // covers case 1
        new_node->prev = NULL;
        new_node->next = NULL;
        list->head = new_node;
        // pointer drawing breakpoint
    } else if (index <= 0) { // covers case 2
        new_node->prev = NULL;
        new_node->next = list->head;
        list->head->prev = new_node;
        list->head = new_node;
        // pointer drawing breakpoint
    } else {
        dplist_node_t *ref_at_index = dpl_get_reference_at_index(list, index);
        assert(ref_at_index != NULL);
        // pointer drawing breakpoint
        if (index < dpl_size(list)) { // covers case 4
            new_node->prev = ref_at_index->prev;
            new_node->next = ref_at_index;
            ref_at_index->prev->next = new_node;
            ref_at_index->prev = new_node;
            // pointer drawing breakpoint
        } else { // covers case 3
            assert(ref_at_index->next == NULL);
            new_node->next = NULL;
            new_node->prev = ref_at_index;
            ref_at_index->next = new_node;
            // pointer drawing breakpoint
        }
    }
    return list;
}

dplist_t *dpl_remove_at_index(dplist_t *list, int index, bool free_element) {
    if (list == NULL) return NULL;
    if (list->head == NULL) return list;

    // First we find the node and move the links
    // Same logic as exercises
    dplist_node_t *node_to_remove;
    if (index <= 0) { // remove first node
        node_to_remove = list->head;
        list->head = list->head->next; // move head over one
        if (list->head != NULL) {
            list->head->prev = NULL;
        }
    } else {
        int size = dpl_size(list);
        if (index < size - 1) { // remove node in the middle
            dplist_node_t *mid_ref = dpl_get_reference_at_index(list, index);
            node_to_remove = mid_ref;
            mid_ref->prev->next = mid_ref->next;
            mid_ref->next->prev = mid_ref->prev;
        } else { // remove last node
            dplist_node_t *penult_ref = dpl_get_reference_at_index(list,
                   size - 2);
            node_to_remove = penult_ref->next;
            penult_ref->next = NULL;
        }
    }

    // Now we free the memory
    if (free_element && list->element_free != NULL) {
        list->element_free(&(node_to_remove->element));
    }

    // node is always freed
    free(node_to_remove);
    return list;
}

int dpl_size(dplist_t *list) {
    if (list == NULL) return -1;

    int count = 0;
    dplist_node_t* current_node = list->head;

    while (current_node != NULL) {
        count++;
        current_node = current_node->next;
    }

    return count;
}

void *dpl_get_element_at_index(dplist_t *list, int index) {
    if (list == NULL || list->head == NULL) return NULL;

    dplist_node_t *node_at_index = dpl_get_reference_at_index(list, index);
    if (node_at_index == NULL) return NULL;

    return node_at_index->element;
}

int dpl_get_index_of_element(dplist_t *list, void *element) {
    if (list == NULL) return -1;

    // Loop through list, use the callback compare function
    int index = 0;
    dplist_node_t* current_node = list->head;

    while (current_node != NULL) {
        if (list->element_compare(current_node->element, element) == 0)
            return index;
        current_node = current_node->next;
        index++;
    }

    return -1;
}

dplist_node_t *dpl_get_reference_at_index(dplist_t *list, int index) {
    if (list == NULL || list->head == NULL) return NULL;

    if (index <= 0) return list->head;

    int i = 0;
    dplist_node_t *current_node = list->head;
    while (i < index) {
        if (current_node->next == NULL) return current_node;
        current_node = current_node->next;
        i++;
    }
    return current_node;
}

void *dpl_get_element_at_reference(dplist_t *list, dplist_node_t *reference) {
    if (list == NULL || list->head == NULL) return NULL;
    if (reference == NULL) return NULL;

    // Check if reference exists
    dplist_node_t* current_node = list->head;
    while (current_node != NULL) {
        if (current_node == reference) {
            return reference->element;
        }
        current_node = current_node->next;
    }

    return NULL;
}
