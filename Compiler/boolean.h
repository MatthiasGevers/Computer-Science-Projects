
#ifndef BOOLEAN_H
#define BOOLEAN_H

/**
 * @brief A Boolean type definition.
 */
typedef enum boolean {
	FALSE, /**< symbol for Boolean false */
	TRUE   /**< symbol for Boolean true  */
} Boolean;

/** a macro to return a string value from a boolean; useful for debugging. */
#define BOOLEAN_VALUE(x) ((x) ? "TRUE" : "FALSE")

#endif /* BOOLEAN_H */
