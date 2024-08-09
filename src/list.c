#include "list.h"

List *create_list() {
    List *list = (List *)malloc(sizeof(List));
    list->array = (char **)malloc(LIST_SIZE * sizeof(char *));

    for (int i = 0; i < LIST_SIZE; i++) {
        list->array[i] = NULL;
    }

    return list;
}

int list_put(List *list, char *str) {
    for(int i = 0; i < LIST_SIZE; i++) {
        if(list->array[i] == NULL) {
            list->array[i] = (char *)malloc((strlen(str) + 1) * sizeof(char));
            strcpy(list->array[i], str);
            return 1;
        }
    }

    return 0;
}

int list_contains(List *list, char *str) {
    for(int i = 0; i < LIST_SIZE; i++) {
        if(list->array[i] != NULL && strcmp(list->array[i], str) == 0) {
            return 1;
        }
    }
    return 0;
}

void free_list(List *list) {
    if(list == NULL) {
        return;
    }
    
    for (int i = 0; i < LIST_SIZE; i++) {
        if(list->array[i] != NULL) {
            free(list->array[i]);
        }
    }
    free(list->array);
    free(list);
}