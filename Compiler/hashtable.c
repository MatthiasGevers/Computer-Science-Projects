/**
 * @file    hashtable.c
 * @brief   A generic hash table.
 * @author  W.H.K. Bester (whkbester@cs.sun.ac.za)
 * @date    2023-07-06
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hashtable.h"

#define INITIAL_DELTA_INDEX 4
#define PRINT_BUFFER_SIZE 1024

/** an entry in the hash table */
typedef struct htentry HTentry;
struct htentry {
	void	*key;	   /*<< the key                      */
	void	*value;	   /*<< the value                    */
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
/*static HTentry **talloc(int tsize);*/
static void rehash(HashTab *ht);



/**
 * the array of differences between a power-of-two and the largest prime less
 * than that power-of-two.
 */
unsigned short delta[] = {0,  0, 1, 1, 3, 1, 3, 1,	5, 3,  3, 9,  3,  1, 3,	 19,
						  15, 1, 5, 1, 3, 9, 3, 15, 3, 39, 5, 39, 57, 3, 35, 1};

#define MAX_IDX (sizeof(delta) / sizeof(short))

/* --- hash table interface ------------------------------------------------- */


/**
 * @brief Initializes a hash table with the specified load factor, hash function, and comparison function.
 *
 * @param loadfactor The maximum load factor before rehashing.
 * @param hash A pointer to the hash function.
 * @param cmp A pointer to the comparison function.
 * @return A pointer to the initialized hash table or NULL if initialization fails.
 */
HashTab *ht_init(
	float loadfactor,
	unsigned int (*hash)(void *, unsigned int),
	int (*cmp)(void *, void *))
{
	HashTab		*ht;
	unsigned int i;

	ht = malloc(sizeof(HashTab));
	if (ht == NULL)
	{
		return NULL;
	}

	ht->size = (1 << INITIAL_DELTA_INDEX) - delta[INITIAL_DELTA_INDEX];

	ht->table = malloc(sizeof(HTentry *) * ht->size);

	if (ht->table == NULL)
	{
		free(ht);
		return NULL;
	}
	ht->idx			   = INITIAL_DELTA_INDEX;
	ht->max_loadfactor = loadfactor;

	ht->hash = hash;
	ht->cmp	 = cmp;

	ht->num_entries = 0;

	for (i = 0; i < ht->size; i++)
	{
		ht->table[i] = NULL;
	}

	return ht;
}


/**
 * @brief Inserts a key-value pair into the hash table.
 *
 * @param ht A pointer to the hash table.
 * @param key The key to insert.
 * @param value The value associated with the key.
 * @return 0 if successful, 1 if memory allocation fails, and HASH_TABLE_KEY_VALUE_PAIR_EXISTS if the key already exists.
 */
int ht_insert(HashTab *ht, void *key, void *value)
{
	int      k;
	HTentry *p;


	if (ht_search(ht, key) != NULL)
	{
		return HASH_TABLE_KEY_VALUE_PAIR_EXISTS;
	}

	ht->num_entries++;
	if (ht->max_loadfactor < (float)ht->num_entries / ht->size
		&& ht->idx < MAX_IDX)
	{
		rehash(ht);
	}


	k = ht->hash(key, ht->size);

	p = malloc(sizeof(HTentry));
	if (!p)
	{
		return 1;
	}

	p->key	 = key;
	p->value = value;

	p->next_ptr = ht->table[k];
	ht->table[k] = p;

	return 0;
}


/**
 * @brief Searches for a key in the hash table and returns the associated value.
 *
 * @param ht A pointer to the hash table.
 * @param key The key to search for.
 * @return A pointer to the value associated with the key, or NULL if the key is not found.
 */
void *ht_search(HashTab *ht, void *key)
{
	int		 k;
	HTentry *p;

	if (!(ht && key))
	{
		return NULL;
	}

	k = ht->hash(key, ht->size);
	for (p = ht->table[k]; p; p = p->next_ptr)
	{
		if (ht->cmp(key, p->key) == 0)
		{
			return p->value;
		}
	}

	return NULL;
}


/**
 * @brief Frees the memory allocated for the hash table and its elements.
 *
 * @param ht A pointer to the hash table.
 * @param freekey A pointer to the function used to free keys (can be NULL).
 * @param freeval A pointer to the function used to free values (can be NULL).
 * @return EXIT_SUCCESS if successful, EXIT_FAILURE if any parameter is NULL.
 */
int ht_free(HashTab *ht, void (*freekey)(void *k), void (*freeval)(void *v))
{
	unsigned int i;
	HTentry		*p, *q;

	if (!(ht && freekey && freeval))
	{
		return EXIT_FAILURE;
	}

	for (i = 0; i < ht->size; i++)
	{
		p = ht->table[i];

		while (p)
		{
			q = p->next_ptr;

			if (freekey)
			{
				freekey(p->key);
			}
			if (freeval)
			{
				freeval(p->value);
			}

			free(p);
			p = q;
		}
	}

	if (ht->table)
	{
		free(ht->table);
		ht->table = NULL;
	}

	if (ht)
	{
		free(ht);
		ht = NULL;
	}


	return EXIT_SUCCESS;
}

/**
 * @brief Prints the contents of the hash table.
 *
 * @param ht A pointer to the hash table.
 * @param keyval2str A pointer to a function that converts key-value pairs to strings (can be NULL).
 */
void ht_print(HashTab *ht, void (*keyval2str)(void *k, void *v, char *b))
{
	unsigned int i;
	HTentry		*p;
	char		 buffer[PRINT_BUFFER_SIZE];

	/* TODO: This function is complete and useful for testing, but you have to
	 * write your own keyval2str function if you want to use it.
	 */

	if (ht && keyval2str)
	{
		for (i = 0; i < ht->size; i++)
		{
			printf("bucket[%2i]", i);
			for (p = ht->table[i]; p != NULL; p = p->next_ptr)
			{
				keyval2str(p->key, p->value, buffer);
				printf(" --> %s", buffer);
			}
			printf(" --> NULL\n");
		}
	}
}

/* --- utility functions ---------------------------------------------------- */

/* TODO: I suggest completing the following helper functions for use in the
 * global functions ("exported" as part of this unit's public API) given above.
 * You may, however, elect not to use them, and then go about it in your own way
 * entirely.  The ball is in your court, so to speak, but remember: I have
 * suggested using these functions for a reason -- they should make your life
 * easier.
 */

/**
 * @brief Increases the size of the hash table.
 *
 * @param ht A pointer to the hash table.
 * @return The new size of the hash table.
 */
static int getsize(HashTab *ht)
{
	ht->idx++;
	return (1 << ht->idx) - delta[ht->idx];
}

/*
static HTentry **talloc(int tsize)
{

}
*/



/**
 * @brief Rehashes the hash table to accommodate more entries.
 *
 * @param ht A pointer to the hash table.
 */
static void rehash(HashTab *ht)
{
	unsigned int i;
	int			 k, size;
	HTentry		*p, *next;
	HTentry	   **new_table;

	size = getsize(ht);

	new_table = (HTentry **)malloc(sizeof(HTentry *) * size);
	if (!new_table)
	{

		return;
	}

	for (k = 0; k < size; k++)
	{
		new_table[k] = NULL;
	}

	for (i = 0; i < ht->size; i++)
	{
		p = ht->table[i];

		while (p)
		{
			next = p->next_ptr;

			k = ht->hash(p->key, size);

			p->next_ptr	 = new_table[k];
			new_table[k] = p;

			p = next;
		}
	}

	free(ht->table);
	ht->table = new_table;
	ht->size  = size;

}
