#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TABLE_SIZE 100

typedef struct Entry {
    int key;
    void *value;
    struct Entry *next;
} Entry;

typedef struct HashMap {
    Entry **table;
    int size;
} HashMap;

Entry* create_entry(int key, void *value);

HashMap* create_hashmap();

unsigned int hash(int key);

void hashmap_put(HashMap *hashmap, int key, void *value);

void* hashmap_get(HashMap *hashmap, int key);

void hashmap_delete(HashMap *hashmap, int key);

int hashmap_size(HashMap *hashmap);

Entry** hashmap_get_all_entries(HashMap *hashmap, int *num_entries);

void free_hashmap(HashMap *hashmap);

#endif