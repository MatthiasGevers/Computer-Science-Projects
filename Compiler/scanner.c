/**
 * @file    scanner.c
 * @brief   The scanner for AMPL-2023.
 * @author  W.H.K. Bester (whkbester@cs.sun.ac.za)
 * @date    2023-06-29
 */

#include "scanner.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "boolean.h"
#include "error.h"
#include "token.h"

/* --- global static variables ---------------------------------------------- */

static FILE *src_file;		/* the source file pointer             */
static int	 ch;			/* the next source character           */
static int	 column_number; /* the current column number           */
static struct {
	char	 *word; /* the reserved word, i.e., the lexeme */
	TokenType type; /* the associated topen type           */
} reserved[] = {
	{"and", TOK_AND},		  {"array", TOK_ARRAY},
	{"bool", TOK_BOOL},		  {"chillax", TOK_CHILLAX},
	{"elif", TOK_ELIF},		  {"else", TOK_ELSE},
	{"end", TOK_END},		  {"false", TOK_FALSE},
	{"if", TOK_IF},			  {"input", TOK_INPUT},
	{"int", TOK_INT},		  {"let", TOK_LET},
	{"main", TOK_MAIN},		  {"not", TOK_NOT},
	{"or", TOK_OR},			  {"output", TOK_OUTPUT},
	{"program", TOK_PROGRAM}, {"rem", TOK_REM},
	{"return", TOK_RETURN},	  {"true", TOK_TRUE},
	{"while", TOK_WHILE}

};

#define NUM_RESERVED_WORDS (sizeof(reserved) / sizeof(reserved[0]))
#define MAX_INIT_STR_LEN (1024)

/* --- function prototypes -------------------------------------------------- */

static void next_char(void);
static void process_number(Token *token);
static void process_string(Token *token);
static void process_word(Token *token);
static void skip_comment(void);

/* --- scanner interface ---------------------------------------------------- */

void init_scanner(FILE *in_file)
{
	src_file	  = in_file;
	position.line = 1;
	position.col = column_number = 0;
	next_char();
}

/**
 * @brief Reads and processes the next token from the file.
 *
 * This function reads characters from the file and processes them to
 * generate tokens. It handles various types of tokens including words, numbers,
 * strings, and special characters. It also skips whitespace and comments
 * as needed.
 *
 * @param token A pointer to the Token structure.
 * @return void
 */

void get_token(Token *token)
{
	while (ch == '{' || isspace(ch) || ch == '\n')
	{
		while (isspace(ch))
		{
			next_char();
		}

		if (ch == '{')
		{
			skip_comment();
			next_char();
		}

		if (ch == '\n')
		{
			next_char();
		}
	}

	/* remember token start */
	position.col = column_number;

	/* get the next token */
	if (ch != EOF)
	{
		if (isalpha(ch) || ch == '_')
		{
			/* process a word */
			process_word(token);

		} else if (isdigit(ch))
		{
			/* process a number */
			process_number(token);

		} else
			switch (ch)
			{
				/* process a string */
				case '"':

					next_char();
					process_string(token);
					break;

				case '=':

					next_char();
					token->type = TOK_EQ;
					break;

				case '>':
					next_char();
					if (ch == '=')
					{
						token->type = TOK_GE;
						next_char();
					} else
					{
						token->type = TOK_GT;
					}
					break;

				case '<':
					next_char();
					if (ch == '=')
					{
						token->type = TOK_LE;
						next_char();
					} else
					{
						token->type = TOK_LT;
					}
					break;

				case '/':

					next_char();
					if (ch == '=')
					{
						next_char();
						token->type = TOK_NE;

					} else
					{
						token->type = TOK_DIV;
					}
					break;

				case '-':
					next_char();
					if (ch == '>')
					{
						token->type = TOK_ARROW;
						next_char();
					} else
					{
						token->type = TOK_MINUS;
					}
					break;

				case '|':
					next_char();
					if (ch == '|')
					{
						next_char();
						token->type = TOK_OR;
					} else
					{
						position.col = column_number - 1;
						leprintf(
							"illegal character '%c' (ASCII #%d)", '|', '|');
					}
					break;

				case '+':

					next_char();
					token->type = TOK_PLUS;
					break;

				case '&':

					next_char();
					if (ch == '&')
					{
						next_char();
						token->type = TOK_AND;
					} else
					{
						position.col = column_number - 1;
						leprintf(
							"illegal character '%c' (ASCII #%d)", '&', '&');
					}
					break;

				case '*':
					next_char();

					token->type = TOK_MUL;
					break;

				case '%':

					next_char();
					token->type = TOK_REM;
					break;

				case ':':
					next_char();
					token->type = TOK_COLON;
					break;

				case ',':

					next_char();
					token->type = TOK_COMMA;
					break;

				case '.':

					next_char();
					if (ch == '.')
					{
						next_char();
						token->type = TOK_DOTDOT;
					} else
					{
						position.col = column_number - 1;
						leprintf(
							"illegal character '%c' (ASCII #%d)", '.', '.');
					}
					break;

				case '[':

					next_char();
					token->type = TOK_LBRACK;
					break;

				case '(':

					next_char();
					token->type = TOK_LPAREN;
					break;

				case ']':

					next_char();
					token->type = TOK_RBRACK;
					break;

				case ')':

					next_char();
					token->type = TOK_RPAREN;
					break;

				case ';':

					next_char();
					token->type = TOK_SEMICOLON;
					break;

				default:
					position.col = column_number;
					leprintf("illegal character '%c' (ASCII #%d)", ch, ch);
					break;
			}

	} else
	{
		position.col--;
		token->type = TOK_EOF;
	}
}

/* --- utility functions ---------------------------------------------------- */

/**
 * Gets the next character from the file and updates position.
 *
 * @return void
 */

void next_char(void)
{
	static char last_read = '\0';

	last_read = ch;
	ch		  = fgetc(src_file);
	if (ch == EOF)
	{
		column_number++;
		return;
	}

	if (last_read == '\n')
	{
		position.line++;
		column_number = 1;
	} else
	{
		column_number++;
	}
}

/**
 * @brief Processes a numeric literal, generating a number token.
 *
 * This function processes characters to form a numeric literal token, ensuring
 * the generated token adheres to the required format. It checks for overflow,
 * and sets the resulting numeric token.
 *
 * @param token A pointer to the Token structure.
 * @return void
 */

void process_number(Token *token)
{
	int number;
	int nextnum;
	number	= ch - '0';
	nextnum = 0;

	next_char();

	while (isdigit(ch))
	{
		nextnum = ch - '0';
		if (number > (INT_MAX - nextnum) / 10)
		{
			leprintf("number too large");
		}
		number = 10 * number + nextnum;

		next_char();
	}

	token->type	 = TOK_NUM;
	token->value = number;
}

/**
 * @brief Processes a string literal, generating a string token.
 *
 * This function processes characters to form a string literal token, handling
 * escape sequences, dynamic memory allocation, and error conditions.
 *
 *
 * @param token A pointer to the Token structure.
 * @return void
 */
void process_string(Token *token)
{
	size_t i, nstring = MAX_INIT_STR_LEN;
	i = 0;
	int pos;
	pos = column_number - 1;

	char *my_string = (char *)malloc((nstring + 1) * sizeof(char));
	if (my_string == NULL)
	{
		leprintf("Memory reallocation failed.\n");
	}

	while (ch != '"')
	{
		if (ch == EOF)
		{
			leprintf("string not closed");
		}
		if (i >= nstring)
		{
			nstring *= 2;
			my_string
				= (char *)realloc(my_string, (nstring + 1) * sizeof(char));

			if (my_string == NULL)
			{
				leprintf("Memory reallocation failed.\n");
			}
		}

		if (!(isprint(ch)))
		{
			position.col = column_number;
			leprintf("non-printable character (ASCII #%d) in string", ch);
		}

		if (ch == '\\')
		{
			my_string[i] = ch;
			i++;
			next_char();

			if (ch == 'n' || ch == 't' || ch == '"' || ch == '\\')
			{
				my_string[i] = ch;

			} else
			{
				position.col = column_number - 1;
				leprintf("illegal escape code '\\%c' in string", ch);
			}
		} else
		{
			my_string[i] = ch;
		}

		i++;
		next_char();
	}
	next_char();

	my_string[i] = '\0';
	position.col = pos;

	token->type	  = TOK_STR;
	token->string = my_string;
}

/**
 * @brief Processes an identifier or keyword, and creates a corresponding token.
 *
 * This function processes characters to form an identifier or keyword token,
 * handling the case where the identifier is too long. It compares the generated
 * lexeme to a list of reserved keywords using binary search to determine if the
 * token should be of a specific keyword type.
 *
 *
 * @param token A pointer to the Token structure.
 * @return void
 */
void process_word(Token *token)
{
	char lexeme[MAX_ID_LEN + 1];
	int	 i, cmp, low, mid, high;
	i = 0;

	while (isalpha(ch) || ch == '_' || isdigit(ch))
	{
		if (i == MAX_ID_LEN)
		{
			leprintf("identifier too long");
		}
		lexeme[i] = ch;
		i++;
		next_char();
	}
	lexeme[i] = '\0';

	low			= 0;
	high		= sizeof(reserved) / sizeof(reserved[0]) - 1;
	token->type = TOK_ID;
	strcpy(token->lexeme, lexeme);

	/*
	 * Performs a binary search to match a lexeme with reserved keywords.
	 */
	while (low <= high)
	{
		mid = low + (high - low) / 2;
		cmp = strcmp(lexeme, reserved[mid].word);

		if (cmp == 0)
		{
			strcpy(token->lexeme, "");

			token->type = reserved[mid].type;
			break;
		} else if (cmp < 0)
		{
			high = mid - 1;
		} else
		{
			low = mid + 1;
		}
	}
}

/**
 * Skips a comment block in the source code.
 *
 * This function skips over comment blocks, recursively handling nested blocks.
 *
 * @return void
 */
void skip_comment(void)
{
	SourcePos start_pos;

	start_pos.col  = column_number;
	start_pos.line = position.line;
	next_char();

	while (ch != '}')
	{
		if (ch == EOF)
		{
			position = start_pos;
			leprintf("comment not closed");
		}
		if (ch == '{')
		{
			skip_comment();
		}

		next_char();
	}
}
