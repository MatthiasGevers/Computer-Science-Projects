/**
 * @file    token.c
 * @brief   Utility functions for AMPL-2023 tokens.
 */

#include "token.h"

#include <assert.h>

/* the token strings */
static char *token_names[] = {"end-of-file",
                              "identifier",
                              "numeric literal",
                              "string literal",
                              "'array'",
                              "'bool'",
                              "'chillax'",
                              "'elif'",
                              "'else'",
                              "'end'",
                              "'if'",
                              "'input'",
                              "'int'",
                              "'let'",
                              "'main'",
                              "'output'",
                              "'program'",
                              "'return'",
                              "'while'",
                              "'false'",
                              "'true'",
                              "'not'",
                              "'='",
                              "'>='",
                              "'>'",
                              "'<='",
                              "'<'",
                              "'/='",
                              "'-'",
                              "'or'",
                              "'+'",
                              "'and'",
                              "'/'",
                              "'*'",
                              "'rem'",
                              "'->'",
                              "':'",
                              "','",
                              "'..'",
                              "'['",
                              "'('",
                              "']'",
                              "')'",
                              "';'"};

/* --- functions ------------------------------------------------------------ */

const char *get_token_string(TokenType type)
{
	assert(type >= 0 && type < (sizeof(token_names) / sizeof(token_names[0])));
	return token_names[type];
}
