/**
 * @file    symboltable.c
 * @brief   A symbol table for AMPL-2023.
 * @author  W.H.K. Bester (whkbester@cs.sun.ac.za)
 * @date    2023-07-06
 */

#include "symboltable.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "boolean.h"
#include "error.h"
#include "hashtable.h"
#include "token.h"

/* --- global static variables ---------------------------------------------- */

static HashTab *table, *saved_table;

static unsigned int curr_offset;

/* --- function prototypes -------------------------------------------------- */

static void			valstr(void *key, void *p, char *str);
/*static void			freeprop(IDPropt *p);*/
static unsigned int shift_hash(void *key, unsigned int size);
static int			key_strcmp(void *val1, void *val2);

static void free_value(void *v);
static void free_key(void *v);
void		keyval2str(void *k, void *v, char *b);

/* --- symbol table interface ----------------------------------------------- */

/**
 * @brief Initializes the symbol table.
 */
void init_symbol_table(void)
{
	saved_table = NULL;
	if ((table = ht_init(0.75f, shift_hash, key_strcmp)) == NULL)
	{
		eprintf("Symbol table could not be initialised");
	}
	curr_offset = 1;
}

/**
 * @brief Opens a subroutine and initializes a new symbol table.
 *
 * @param id The identifier of the subroutine.
 * @param prop Pointer to the ID property.
 * @return TRUE if successful, FALSE otherwise.
 */
Boolean open_subroutine(char *id, IDPropt *prop)
{
	HashTab *temp;
	if (!insert_name(id, prop))
	{
		return FALSE;
	}

	saved_table = table;

	if ((temp = ht_init(0.75f, shift_hash, key_strcmp)) == NULL)
	{
		eprintf("Symbol table could not be initialised");
	}
	curr_offset = 0;

	table = temp;

	return TRUE;

}

/**
 * @brief Closes the current subroutine and restores the previous symbol table.
 */
void close_subroutine(void)
{
	ht_free(table, free_key, free_value);
	curr_offset = 1;
	table = saved_table;
}

/**
 * @brief Inserts an identifier into the symbol table.
 *
 * @param id The identifier to insert.
 * @param prop Pointer to the ID property.
 * @return TRUE if successful, FALSE otherwise.
 */
Boolean insert_name(char *id, IDPropt *prop)
{
	IDPropt *temp = NULL;
	if (find_name(id, &temp))
	{
		return FALSE;
	}
	if (ht_insert(table, id, prop) != 0)
	{
		return FALSE;
	}
	prop->offset = curr_offset;
	if (IS_VARIABLE(prop->type))
	{
		curr_offset++;
	}
	return TRUE;
}


/**
 * @brief Finds an identifier in the symbol table.
 *
 * @param id The identifier to search for.
 * @param prop Pointer to the ID property.
 * @return TRUE if found, FALSE otherwise.
 */
Boolean find_name(char *id, IDPropt **prop)
{
	/* TODO: Nothing, unless you want to. */
	*prop = ht_search(table, id);
	if (!*prop && saved_table)
	{
		*prop = ht_search(saved_table, id);
		if (*prop && !IS_CALLABLE_TYPE((*prop)->type))
		{
			*prop = NULL;
		}
	}

	return (*prop ? TRUE : FALSE);
}

int get_variables_width(void) { return curr_offset; }

void release_symbol_table(void)
{
	ht_free(saved_table, free_key, free_value);
	/* TODO: Free the underlying structures of the symbol table. */
}

void print_symbol_table(void) { ht_print(table, valstr); }

/* --- utility functions ---------------------------------------------------- */

static void valstr(void *key, void *p, char *str)
{
	char	*keystr = (char *)key;
	IDPropt *idpp	= (IDPropt *)p;

	/* TODO: Nothing, but this should give you an idea of how to look at the
	 * content of the symbol table.
	 */

	if (IS_CALLABLE_TYPE(idpp->type))
	{
		sprintf(str, "%s@_[%s]", keystr, get_valtype_string(idpp->type));
	} else {
		sprintf(str, "%s@%d[%s]", keystr,
			idpp->offset,
			get_valtype_string(idpp->type));
	}
}


/*
static void freeprop(IDPropt *p)
{
	if (p == NULL)
	{
		return;
	}

	if (p->params != NULL)
	{
		free(p->params);
	}

	free(p);
}
*/


/**
 * @brief Compares two strings as keys for a hash table.
 *
 * @param val1 The first string.
 * @param val2 The second string.
 * @return An integer less than, equal to, or greater than zero if val1
 * is found to be less than, equal to, or greater than val2, respectively.
 */
static int key_strcmp(void *val1, void *val2)
{
	char *str1;
	char *str2;

	str1 = (char *)val1;
	str2 = (char *)val2;

	return strcmp(str1, str2);
}


/**
 * @brief Computes a hash value for a given key using a
 * shift-based hash function.
 * @param key The key for which to compute the hash value.
 * @param size The size of the hash table.
 * @return The computed hash value as an unsigned integer.
 */
static unsigned int shift_hash(void *key, unsigned int size)
{
#ifdef DEBUG_SYMBOL_TABLE
	char		*keystr = (char *)key;
	unsigned int i, hash, length;

	hash   = 0;
	length = strlen(keystr);
	for (i = 0; i < length; i++)
	{
		hash += keystr[i];
	}

	return (hash % size);
#else
	/* TODO insert your actual hash function here */

	char *id;
	int	  hash, i;

	id	 = (char *)key;
	hash = 0;

	for (i = 0; i < (int)strlen(id); i++)
	{
		hash = (hash << 5 | hash >> 27);
		hash += id[i];
	}

	return (hash % size);

#endif /* DEBUG_SYMBOL_TABLE */
}

void keyval2str(void *k, void *v, char *b)
{
	char *key_str	= (char *)k;
	char *value_str = (char *)v;

	snprintf(b, 29, "Key: %s, Value: %s", key_str, value_str);
}

static void free_value(void *v) { free(v); }

static void free_key(void *v) { free(v); }
