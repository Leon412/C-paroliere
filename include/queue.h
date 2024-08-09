#ifndef QUEUE_H
#define QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node  {
    void *value;
    struct Node *next;
} Node;

typedef struct Queue {
    struct Node *head;
    struct Node *tail;
    int size;
} Queue;

Queue *queue_create();

void queue_push();

void *queue_pop();

int queue_size();

void free_queue();

#endif