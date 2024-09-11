/**
 * @file    scanner.h
 * @brief   Definitions for the exported variables and functions for the
 *          lexical analyser (scanner) of AMPL-2023.
 */

#ifndef SCANNER_H
#define SCANNER_H

#include "token.h"

#include <stdio.h>

/**
 * Initialise the scanner.
 *
 * @param[in]   in_file
 *     the (already open) source file
 */
void init_scanner(FILE *in_file);

/**
 * Get the next token from the input (source) file.
 *
 * @param[out]  token
 *     contains the token just scanned
 */
void get_token(Token *token);

#endif /* SCANNER_H */
