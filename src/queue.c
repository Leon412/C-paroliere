#include "queue.h"

Queue *queue_create() {
    Queue *queue = (Queue *)malloc(sizeof(Queue));
    if (!queue) {
        perror("malloc");
        return NULL;
    }
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    return queue;
}

void queue_push(Queue *queue, void *value) {
    struct Node *new_element = (struct Node *)malloc(sizeof(struct Node));
    if (!new_element) {
        perror("malloc");
        return;
    }
    new_element->value = value;
    new_element->next = NULL;
    
    if(queue->head == NULL && queue->tail == NULL) {
        queue->head = new_element;
        queue->tail = new_element;
    }
    else {
        queue->tail->next = new_element;
        queue->tail = new_element;
    }
    queue->size++;
}

void *queue_pop(Queue *queue) {
    if (queue->head == NULL) {
        return NULL;
    }
    Node *front_element = queue->head;
    void *value = front_element->value;
    queue->head = front_element->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    free(front_element);
    queue->size--;
    return value;
}

int queue_size(Queue *queue) {
    return queue->size;
}

void free_queue(Queue *queue) {
    while (queue->head != NULL) {
        Node *front_element = queue->head;
        queue->head = queue->head->next;
        free(front_element);
    }
    free(queue);
}