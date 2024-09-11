/**
 * @file    hashtable.c
 * @brief   A generic hash table.
 */

#include "hashtable.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_DELTA_INDEX 4
#define PRINT_BUFFER_SIZE   1024

/** an entry in the hash table */
typedef struct htentry HTentry;
struct htentry {
	void *key;         /*<< the key                      */
	void *value;       /*<< the value                    */
	HTentry *next_ptr; /*<< the next entry in the bucket */
};

/** a hash table container */
struct hashtab {
	/** a pointer to the underlying table                              */
	HTentry **table;
	/** the current size of the underlying table                       */
	unsigned int size;
	/** the current number of entries                                  */
	unsigned int num_entries;
	/** the maximum load factor before the underlying table is resized */
	float max_loadfactor;
	/** the index into the delta array                                 */
	unsigned short idx;
	/** a pointer to the hash function                                 */
	unsigned int (*hash)(void *, unsigned int);
	/** a pointer to the comparison function                           */
	int (*cmp)(void *, void *);
};

/* --- function prototypes -------------------------------------------------- */


static int getsize(HashTab *ht);
static HTentry **talloc(int tsize);
static void rehash(HashTab *ht);



/**
 * the array of differences between a power-of-two and the largest prime less
 * than that power-of-two.
 */
unsigned short delta[] = {0,  0, 1, 1, 3, 1, 3, 1,  5, 3,  3, 9,  3,  1, 3,  19,
                          15, 1, 5, 1, 3, 9, 3, 15, 3, 39, 5, 39, 57, 3, 35, 1};

#define MAX_IDX (sizeof(delta) / sizeof(short))

/* --- hash table interface ------------------------------------------------- */

HashTab *ht_init(float loadfactor,
                 unsigned int (*hash)(void *, unsigned int),
                 int (*cmp)(void *, void *))
{
	HashTab *ht;
	unsigned int i;


	return ht;
}

int ht_insert(HashTab *ht, void *key, void *value)
{
	int k;
	HTentry *p;

	return EXIT_SUCCESS;
}

void *ht_search(HashTab *ht, void *key)
{
	int k;
	HTentry *p;


	if (!(ht && key)) {
		return NULL;
	}

	k = ht->hash(key, ht->size);
	for (p = ht->table[k]; p; p = p->next_ptr) {
		if (ht->cmp(key, p->key) == 0) {
			return p->value;
		}
	}

	return NULL;
}

int ht_free(HashTab *ht, void (*freekey)(void *k), void (*freeval)(void *v))
{
	unsigned int i;
	HTentry *p, *q;

	if (!(ht && freekey && freeval)) {
		return EXIT_FAILURE;
	}

	

	return EXIT_SUCCESS;
}

void ht_print(HashTab *ht, void (*keyval2str)(void *k, void *v, char *b))
{
	unsigned int i;
	HTentry *p;
	char buffer[PRINT_BUFFER_SIZE];


	if (ht && keyval2str) {
		for (i = 0; i < ht->size; i++) {
			printf("bucket[%2i]", i);
			for (p = ht->table[i]; p != NULL; p = p->next_ptr) {
				keyval2str(p->key, p->value, buffer);
				printf(" --> %s", buffer);
			}
			printf(" --> NULL\n");
		}
	}
}

/* --- utility functions ---------------------------------------------------- */



static int getsize(HashTab *ht)
{
	
}

static HTentry **talloc(int tsize)
{
	
}

static void rehash(HashTab *ht)
{

}
