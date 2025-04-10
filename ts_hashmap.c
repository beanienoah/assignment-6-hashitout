#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ts_hashmap.h"


// considerations:
// -> spinlocks better? 
//    a. we had more experience with them in class
//    b. these have short critical sections, unless i'm stupid
//    -> if slower then sol. just replace ig
//    -> adp mutex?
// -> lmao this is really making me appreciate "->"
// -> ask about #ops discrepencies
// -> THAT FIXED IT
// -> made a lock for numOps :D (dunno how i didn't think of that)

/**
 * Creates a new thread-safe hashmap. 
 *
 * @param capacity initial capacity of the hashmap.
 * @return a pointer to a new thread-safe hashmap.
 */
ts_hashmap_t *initmap(int capacity) {
  // TODO: set up (re-add lock stuff just get it working for now)
  // doesn't need to be thread safe, sets up ts map
  ts_hashmap_t *map = malloc(sizeof(ts_hashmap_t)); // hashmap
  map->capacity = capacity;
  map->table = malloc(sizeof(ts_entry_t *) * capacity); // table (i smell problems)
  map->numOps = 0;
  map->speen = malloc(sizeof(pthread_spinlock_t) * capacity); // splocks
  // numOps lock (test)
  pthread_spin_init(&map->numOpsLock, PTHREAD_PROCESS_PRIVATE);
  
  // one spinlock per
  for(int i = 0; i < capacity; i++) {
    pthread_spin_init(&map->speen[i], PTHREAD_PROCESS_PRIVATE); // pshared private (one process)
  }

  return map; // definitely thread safe...
}

/**
 * Obtains the value associated with the given key.
 * @param map a pointer to the map
 * @param key a key to search
 * @return the value associated with the given key, or INT_MAX if key not found
 */
int get(ts_hashmap_t *map, int key) {
  // TODO: this basically became a template-ish for the other funcs
  // key as hash: capacity 10 (0-9 bins)
  // key = 99 % 10 = bin 3
  int idx = key % map->capacity; // idx for key
  pthread_spin_lock(&map->speen[idx]); // lock this bin
  ts_entry_t *current = map->table[idx]; // traverse...
  // if key exists
  while(current != NULL) {
    if(current->key == key) { // matching key found
      int v = current->value; // grab value
      pthread_spin_lock(&map->numOpsLock);
      map->numOps++; // increment
      pthread_spin_unlock(&map->numOpsLock);
      pthread_spin_unlock(&map->speen[idx]); // unlock it
      return v;
    }
    current = current->next; // next entry
  }
  // key not exist
  // okay so to make sure numOps is consisitent, lock it before updating
  pthread_spin_lock(&map->numOpsLock);
  map->numOps++;
  // and then unlock it
  pthread_spin_unlock(&map->numOpsLock);
  pthread_spin_unlock(&map->speen[idx]); // unlock it
  return INT_MAX;
}

/**
 * Associates a value associated with a given key.
 * @param map a pointer to the map
 * @param key a key
 * @param value a value
 * @return old associated value, or INT_MAX if the key was new
 */
int put(ts_hashmap_t *map, int key, int value) {
  // TODO
  int idx = key % map->capacity;
  pthread_spin_lock(&map->speen[idx]); // lock it up
  ts_entry_t *current = map->table[idx];
  
  // key found
  while(current != NULL) {
    if(current->key == key) {
      int previous = current->value; // store prev value
      current->value = value; // update
      pthread_spin_lock(&map->numOpsLock);
      map->numOps++;
      pthread_spin_unlock(&map->numOpsLock);
      pthread_spin_unlock(&map->speen[idx]); // unlock it
      return previous; // return old value
    }
    current = current->next; // next entry
  }

  // key not found, key is new
  ts_entry_t *hero = malloc(sizeof(ts_entry_t));
  hero->value = value;
  hero->key = key;
  hero->next = map->table[idx]; // insert
  map->table[idx] = hero; 
  pthread_spin_lock(&map->numOpsLock);
  map->numOps++;
  pthread_spin_unlock(&map->numOpsLock);
  pthread_spin_unlock(&map->speen[idx]); // bruh okay im dumb
  return INT_MAX;
}

/**
 * Removes an entry in the map
 * @param map a pointer to the map
 * @param key a key to search
 * @return the value associated with the given key, or INT_MAX if key not found
 */
int del(ts_hashmap_t *map, int key) {
  // TODO
  int idx = key % map->capacity;
  pthread_spin_lock(&map->speen[idx]);
  ts_entry_t *current = map->table[idx];
  ts_entry_t *previous = NULL; // previous "node"
  
  while(current != NULL) {
    if(current->key == key) {
      int v = current->value; // store value
      if(previous == NULL) {
        map->table[idx] = current->next; // remove first
      } else {
        previous->next = current->next; // remove others
      }
      pthread_spin_lock(&map->numOpsLock);
      map->numOps++;
      pthread_spin_unlock(&map->numOpsLock);
      pthread_spin_unlock(&map->speen[idx]);
      return v; // return previously associated value 
    }
    previous = current;
    current = current->next; // next entry
  }

  pthread_spin_lock(&map->numOpsLock);
  map->numOps++;
  pthread_spin_unlock(&map->numOpsLock);
  pthread_spin_unlock(&map->speen[idx]);
  return INT_MAX;
}


/**
 * Prints the contents of the map (given)
 */
void printmap(ts_hashmap_t *map) {
  for (int i = 0; i < map->capacity; i++) {
    printf("[%d] -> ", i);
    ts_entry_t *entry = map->table[i];
    while (entry != NULL) {
      printf("(%d,%d)", entry->key, entry->value);
      if (entry->next != NULL)
        printf(" -> ");
      entry = entry->next;
    }
    printf("\n");
  }
}

/**
 * Free up the space allocated for hashmap
 * @param map a pointer to the map
 */
void freeMap(ts_hashmap_t *map) {
  // TODO: iterate through each list, free up all nodes
  for(int i = 0; i < map->capacity; i++) {
    ts_entry_t *current = map->table[i];
    while(current != NULL) {
      ts_entry_t *temp = current;
      current = current->next; // next
      free(temp); // free previous
    }
    pthread_spin_destroy(&map->speen[i]); // destroy its spinlock
  }
  // TODO: free the hash table
  pthread_spin_destroy(&map->numOpsLock);
  free(map->speen);
  free(map->table);
  free(map);
  // TADAH
  // TODO: destroy locks (both are destroyed)
}