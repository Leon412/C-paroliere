#include "hashmap.h"

Entry* create_entry(int key, void *value) {
    Entry *new_entry = (Entry *)malloc(sizeof(Entry));
    new_entry->key = key;
    new_entry->value = value;
    new_entry->next = NULL;
    return new_entry;
}

HashMap* create_hashmap() {
    HashMap *hashmap = (HashMap *)malloc(sizeof(HashMap));
    hashmap->table = (Entry **)malloc(TABLE_SIZE * sizeof(Entry *));
    for (int i = 0; i < TABLE_SIZE; i++) {
        hashmap->table[i] = NULL;
    }
    hashmap->size = 0;
    return hashmap;
}

unsigned int hash(int key) {
    return key % TABLE_SIZE;
}

void hashmap_put(HashMap *hashmap, int key, void *value) {
    unsigned int index = hash(key);
    Entry *new_entry = create_entry(key, value);
    if (hashmap->table[index] == NULL) {
        hashmap->table[index] = new_entry;
    } else {
        Entry *temp = hashmap->table[index];
        while (temp->next != NULL && temp->key != key) {
            temp = temp->next;
        }
        if (temp->key == key) {
            temp->value = value;
            free(new_entry);
        } else {
            temp->next = new_entry;
        }
    }
    hashmap->size++;
}

void* hashmap_get(HashMap *hashmap, int key) {
    unsigned int index = hash(key);
    Entry *temp = hashmap->table[index];
    while (temp != NULL && temp->key != key) {
        temp = temp->next;
    }
    if (temp == NULL) {
        return NULL;
    } else {
        return temp->value;
    }
}

void hashmap_delete(HashMap *hashmap, int key) {
    unsigned int index = hash(key);
    Entry *current = hashmap->table[index];
    Entry *prev = NULL;
    
    while (current != NULL && current->key != key) {
        prev = current;
        current = current->next;
    }
    
    if (current == NULL) {
        return;
    }
    
    if (prev == NULL) {
        hashmap->table[index] = current->next;
    } else {
        prev->next = current->next;
    }
    
    free(current);
    hashmap->size--;
}

int hashmap_size(HashMap *hashmap) {
    return hashmap->size;
}

Entry** hashmap_get_all_entries(HashMap *hashmap, int *num_entries) {
    *num_entries = 0;
    Entry **entries = (Entry **)malloc(hashmap->size * sizeof(Entry *));
    if (entries == NULL) {
        return NULL;
    }
    
    for (int i = 0; i < TABLE_SIZE; i++) {
        Entry *current = hashmap->table[i];
        while (current != NULL) {
            entries[*num_entries] = current;
            (*num_entries)++;
            current = current->next;
        }
    }
    
    return entries;
}

void free_hashmap(HashMap *hashmap) {
    for (int i = 0; i < TABLE_SIZE; i++) {
        Entry *temp = hashmap->table[i];
        while (temp != NULL) {
            Entry *to_free = temp;
            temp = temp->next;
            free(to_free);
        }
    }
    free(hashmap->table);
    free(hashmap);
}