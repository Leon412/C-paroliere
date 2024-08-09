#ifndef LIST_H
#define LIST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIST_SIZE 100

typedef struct List {
    char **array;
    int size;
} List;

List *create_list();

int list_put(List *list, char *str);

int list_contains(List *list, char *str);

void free_list(List *list);

#endif