/**
 * @file    amplc.c
 *
 * A recursive-descent compiler for the AMPL-2023 language.
 *
 * All scanning errors are handled in the scanner.  Parser errors MUST be
 * handled by the <code>abort_c</code> function.  System and environment errors
 * -- for example, running out of memory -- MUST be handled in the unit in which
 * they occur.  Transient errors -- for example, nonexistent files, MUST be
 * reported where they occur.  There are no warnings, which is to say, all
 * errors are fatal and MUST cause compilation to terminate with an abnormal
 * error code.
 * this is important
 * @author  W.H.K. Bester (whkbester@cs.sun.ac.za)
 * @date    2023-07-04
 */

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "valtypes.h"
#include "symboltable.h"
#include "codegen.h"
#include "boolean.h"
#include "errmsg.h"
#include "error.h"
#include "scanner.h"
#include "token.h"

/* --- type definitions ----------------------------------------------------- */


typedef struct variable_s Variable;
struct variable_s {
	char	 *id;	/**< variable identifier                                */
	ValType	  type; /**< variable type                                      */
	SourcePos pos;	/**< position of the variable in the source             */
	Variable *next; /**< pointer to the next variable in the list           */
};

/* --- debugging ------------------------------------------------------------ */


#ifdef DEBUG_PARSER
void debug_start(const char *fmt, ...);
void debug_end(const char *fmt, ...);
void debug_info(const char *fmt, ...);
#define DBG_start(...) debug_start(__VA_ARGS__)
#define DBG_end(...) debug_end(__VA_ARGS__)
#define DBG_info(...) debug_info(__VA_ARGS__)
#else
#define DBG_start(...)
#define DBG_end(...)
#define DBG_info(...)
#endif /* DEBUG_PARSER */

/* --- global variables ----------------------------------------------------- */

Token token; /**< the lookahead token type                           */
#if 0
ValType return_type;  /**< the return type of the current subroutine          */
#endif


/* --- helper macros -------------------------------------------------------- */

#define STARTS_FACTOR(toktype)                                        \
	(toktype == TOK_ID || toktype == TOK_NUM || toktype == TOK_LPAREN \
	 || toktype == TOK_NOT || toktype == TOK_TRUE || toktype == TOK_FALSE)

#define STARTS_EXPR(toktype) (toktype == TOK_MINUS || STARTS_FACTOR(toktype))

#define IS_ADDOP(toktype) (toktype >= TOK_MINUS && toktype <= TOK_PLUS)

#define IS_MULOP(toktype)                                           \
	(toktype == TOK_AND || toktype == TOK_DIV || toktype == TOK_MUL \
	 || toktype == TOK_REM)

#define IS_ORDOP(toktype)                                        \
	(toktype == TOK_GE || toktype == TOK_GT || toktype == TOK_LE \
	 || toktype == TOK_LT)

#define IS_RELOP(toktype)                                        \
	(toktype == TOK_EQ || toktype == TOK_GE || toktype == TOK_GT \
	 || toktype == TOK_LE || toktype == TOK_LT || toktype == TOK_NE)

#define IS_TYPE(toktype) (toktype == TOK_BOOL || toktype == TOK_INT)

/* --- function prototypes: parser routines --------------------------------- */

void parse_program(void);
void parse_subdef(void);
void parse_body(void);
void parse_type(ValType *type);
void parse_vardef(void);
void parse_statements(void);
void parse_statement(void);
void parse_assign(void);
void parse_call(void);
void parse_if(void);
void parse_input(void);
void parse_output(void);
void parse_return(void);
void parse_while(void);
void parse_arglist();
void parse_index();
void parse_expr(ValType *type);
void parse_simple(ValType *type);
void parse_term(ValType *type);
void parse_factor(ValType *type);


void chktypes(ValType found, ValType expected, SourcePos *pos, ...);
void expect(TokenType type);
void expect_id(char **id);

/* --- function prototypes: constructors ------------------------------------ */

IDPropt *idpropt(ValType type, unsigned int offset, unsigned int nparams,
ValType *params);
Variable *variable(char *id, ValType type, SourcePos pos);

/* --- function prototypes: error reporting --------------------------------- */

void abort_c(Error err, ...);
void abort_cp(SourcePos *posp, Error err, ...);

/* --- main routine --------------------------------------------------------- */

int main(int argc, char *argv[])
{

	char *jasmin_path;

	FILE *src_file;


	/* set up global variables */
	setprogname(argv[0]);

	/* check command-line arguments and environment */
	if (argc != 2)
	{
		eprintf("usage: %s <filename>", getprogname());
	}

	if ((jasmin_path = getenv("JASMIN_JAR")) == NULL)
	{
		eprintf("JASMIN_JAR environment variable not set");
	}

	setsrcname(argv[1]);

	/* open the source file, and report an error if it cannot be opened */
	if ((src_file = fopen(argv[1], "r")) == NULL)
	{
		eprintf("file '%s' could not be opened:", argv[1]);
	}

	/* initialise all compiler units */
	init_scanner(src_file);

	init_symbol_table();
	init_code_generation();

	get_token(&token);
	parse_program();

	make_code_file();
	assemble(jasmin_path);

#ifdef DEBUG_CODEGEN
	list_code();
#endif

	/* produce the object code, and assemble */
	

	/* release all allocated resources */
	
	release_code_generation();
	release_symbol_table();

	fclose(src_file);
	freeprogname();
	freesrcname();

#ifdef DEBUG_PARSER
	printf("Success!\n");
#endif

	return EXIT_SUCCESS;
}

/* --- parser routines ------------------------------------------------------ */

/*
 * program = "program" id ":" { subdef } "main" ":" body .
 */
void parse_program(void)
{
	char *class_name;

	DBG_start("<program>");


	expect(TOK_PROGRAM);
	expect_id(&class_name);
	set_class_name(class_name);
	expect(TOK_COLON);

	while (token.type == TOK_ID)
	{
		parse_subdef();
	}

	expect(TOK_MAIN);
	expect(TOK_COLON);

	init_subroutine_codegen("main", idpropt(TYPE_CALLABLE, 0, 0, NULL));

	parse_body();
	gen_1(JVM_RETURN);
	close_subroutine_codegen(get_variables_width());

	free(class_name);
	DBG_end("</program>");
}

/**
 * @brief Parses a subdefinition.
 *
 * Parses a subdefinition by extracting identifier, parameter types, return
 * type, and body. It only allows tokens in a specific order according to the
 * EBNF for subdef
 *
 */

void parse_subdef(void)
{
	IDPropt	 *prop;
	char	 *id, *name;
	int		  counter, i;
	ValType	 *type;
	Variable *head, *var;

	var = (Variable *)malloc(sizeof(Variable));

	type = (ValType *)malloc(sizeof(ValType));

	prop = (IDPropt *)malloc(sizeof(IDPropt));

	DBG_start("<subdef>");

	counter = 1;
	expect_id(&name);

	expect(TOK_LPAREN);

	parse_type(type);
	expect_id(&id);
	head = NULL;

	var->id	  = id;
	var->type = *type;
	var->pos  = position;
	var->next = NULL;

	while (token.type == TOK_COMMA)
	{
		get_token(&token);
		counter++;
		Variable *newVariable = (Variable *)malloc(sizeof(Variable));
		parse_type(type);
		newVariable->pos = position;
		expect_id(&id);

		newVariable->id	  = id;
		newVariable->type = *type;

		newVariable->next = NULL;

		if (head == NULL)
		{
			head = newVariable;
		} else {
			Variable *current = head;
			while (current->next != NULL)
			{
				current = current->next;
			}
			current->next = newVariable;
		}
	}

	var->next = head;

	prop->nparams = counter;
	prop->params  = (ValType *)emalloc(sizeof(ValType) * counter + 1);

	for (i = 0; i < counter; i++)
	{
		prop->params[i] = var->type;
		var				= var->next;
	}

	/*IDPropt* idprop = idpropt(returnType, localVarOffset, numParams,
	 * paramTypes);*/

	open_subroutine(name, prop);
	IDPropt *temp = NULL;

	for (i = 0; i < counter; i++)
	{
		temp->type = prop->params[i];
		insert_name(var->id, temp);
		var = var->next;
	}

	expect(TOK_RPAREN);

	if (token.type == TOK_ARROW)
	{
		get_token(&token);
		parse_type(type);
	}

	expect(TOK_COLON);

	init_subroutine_codegen(name, prop);
	parse_body();
	close_subroutine_codegen(get_variables_width());
	close_subroutine();

	DBG_end("</subdef>");
}

/**
 * @brief Parses the body.
 *
 * This function handles the parsing of the body within the program.
 *
 */

void parse_body(void)
{
	DBG_start("<body>");

	while (IS_TYPE(token.type))
	{
		parse_vardef();
	}

	parse_statements();

	DBG_end("</body>");
}

/**
 * @brief Parses the type.
 *
 * This function is used to check if the tokens follow the structure of type.
 *
 */
void parse_type(ValType *type)
{

	DBG_start("<type>");

	if (token.type == TOK_INT)
	{
		*type = TYPE_INTEGER;
	}
	if (token.type == TOK_BOOL)
	{
		*type = TYPE_BOOLEAN;
	}
	get_token(&token);

	if (token.type == TOK_ARRAY)
	{
		SET_AS_ARRAY(*type);
		get_token(&token);
	}

	DBG_end("</type>");
}

/**
 * @brief Parses a variable definition.
 *
 * This function handles the parsing of variable definitions within the program.
 *
 */
void parse_vardef(void)
{
	ValType type;
	DBG_start("<vardef>");
	char *id;

	parse_type(&type);
	expect_id(&id);

	insert_name(id, idpropt(type, 0, 0, NULL));

	while (token.type == TOK_COMMA)
	{
		get_token(&token);
		expect_id(&id);
		insert_name(id, idpropt(type, 0, 0, NULL));
	}
	expect(TOK_SEMICOLON);
	DBG_end("</vardef>");
}

/**
 * @brief Parses multiple statements.
 *
 * This function handles statements within the program.
 *
 */
void parse_statements(void)
{
	DBG_start("<statements>");
	if (token.type == TOK_CHILLAX)
	{
		get_token(&token);
	} else {
		parse_statement();
		while (token.type == TOK_SEMICOLON)
		{
			get_token(&token);
			parse_statement();
		}
	}
	DBG_end("</statements>");
}

/**
 * @brief Parses a single statement.
 *
 * This function parses a single statement according to the definition in the
 * EBNF. It uses a switch statement to call the next function, otherwise it
 * aborts with the correct error message.
 *
 */
void parse_statement(void)
{
	DBG_start("<statement>");

	switch (token.type)
	{
		case TOK_LET:
			parse_assign();
			break;

		case TOK_ID:
			parse_call();
			break;

		case TOK_IF:
			parse_if();
			break;

		case TOK_INPUT:
			parse_input();
			break;

		case TOK_OUTPUT:
			parse_output();
			break;

		case TOK_RETURN:
			parse_return();
			break;

		case TOK_WHILE:
			parse_while();
			break;

		default:
			abort_c(ERR_EXPECTED_STATEMENT);
			break;
	}
	DBG_end("</statement>");
}

/**
 * @brief Parses an assignment statement.
 *
 * This function handles the parsing of assignment statements. If the format
 * of the tokens is wrong it aborts with the correct error message.
 */
void parse_assign(void)
{
	DBG_start("<assign>");
	char	*id;
	IDPropt *propt;
	ValType	 type;

	expect(TOK_LET);
	expect_id(&id);

	find_name(id, &propt);

	type = propt->type;

	if (token.type == TOK_LBRACK)
	{
		gen_2(JVM_ALOAD, propt->offset);
		parse_index();
		SET_BASE_TYPE(type);
	}

	expect(TOK_EQ);

	if (STARTS_EXPR(token.type))
	{

		parse_expr(&type);

		if (IS_ARRAY_TYPE(type))
		{
			gen_2(JVM_ASTORE, propt->offset);
		} else if (IS_ARRAY_TYPE(propt->type)) {
			gen_1(JVM_IASTORE);
		} else {
			gen_2(JVM_ISTORE, propt->offset);
		}
	} else if (token.type == TOK_ARRAY) {
		get_token(&token);
		parse_simple(&type);

		if (IS_INTEGER_TYPE(type))
		{
			gen_newarray(T_INT);
			gen_2(JVM_ASTORE, propt->offset);
		} else {
			gen_newarray(T_BOOLEAN);
			gen_2(JVM_ASTORE, propt->offset);
		}
	} else {
		abort_c(ERR_EXPECTED_EXPRESSION_OR_ARRAY_ALLOCATION);
	}
	/*gen_2(JVM_LDC, token.value);
	gen_2(JVM_ISTORE, propt.offset);*/
	DBG_end("</assign>");
}

/**
 * Parses a function call.
 */
void parse_call(void)
{
	DBG_start("<call>");
	char *id;

	expect_id(&id);
	parse_arglist();
	DBG_end("</call>");
}

/**
 * This function handles the parsing of an if-else statement.
 */
void parse_if(void)
{
	DBG_start("<if>");
	Label	n, c, end;
	ValType type;
	n = get_label();
	c = get_label();

	expect(TOK_IF);

	parse_expr(&type);
	expect(TOK_COLON);
	gen_2_label(JVM_IFEQ, c);
	parse_statements();
	gen_2_label(JVM_GOTO, n);

	gen_label(c);

	while (token.type == TOK_ELIF)
	{
		DBG_info("<elif>");
		get_token(&token);
		end = get_label();

		parse_expr(&type);
		gen_2_label(JVM_IFEQ, end);

		expect(TOK_COLON);
		parse_statements();

		gen_2_label(JVM_GOTO, n);

		gen_label(end);
		DBG_info("</elif>");
	}

	if (token.type == TOK_ELSE)
	{
		DBG_info("<else>");
		get_token(&token);
		expect(TOK_COLON);
		parse_statements();
		DBG_info("</else>");
	}

	gen_label(n);

	expect(TOK_END);
	DBG_end("</if>");
}

/**
 * Parses input according to a specific format.
 *
 * This function parses input the EBNF format, expecting specific tokens
 * and structures.
 *
 */
void parse_input(void)
{
	DBG_start("<input>");
	char	*id;
	IDPropt *p;
	ValType	 v;
	expect(TOK_INPUT);
	expect(TOK_LPAREN);
	expect_id(&id);

	find_name(id, &p);

	v = p->type;

	if (!(p->type == 5 || p->type == 3))
	{
		if (v == TYPE_BOOLEAN || v == TYPE_INTEGER)
		{
			gen_read(v);
			gen_2(JVM_ISTORE, p->offset);
		}
	}

	if (token.type == TOK_LBRACK)
	{

		gen_2(JVM_ALOAD, p->offset);

		parse_index();
		gen_read(SET_BASE_TYPE(v));

		gen_1(JVM_IASTORE);
	}

	expect(TOK_RPAREN);
	DBG_end("</input>");
}

/**
 * Parses output information based on defined format.
 *
 * This function handles the parsing of output information, following a specific
 * format. It expects various token types such as TOK_OUTPUT, TOK_LPAREN,
 * TOK_STR, TOK_DOTDOT, and tokens associated with expressions.
 */
void parse_output()
{
	ValType type;

	DBG_start("<output>");

	expect(TOK_OUTPUT);
	expect(TOK_LPAREN);

	if (token.type == TOK_STR)
	{
		get_token(&token);
		gen_print_string(token.string);
	} else if (STARTS_EXPR(token.type)) {
		parse_expr(&type);

		gen_print(type);

	} else {
		abort_c(ERR_EXPECTED_EXPRESSION_OR_STRING);
	}

	while (token.type == TOK_DOTDOT)
	{
		get_token(&token);
		if (token.type == TOK_STR)
		{
			get_token(&token);
			gen_print_string(token.string);
		} else if (STARTS_EXPR(token.type)) {
			parse_expr(&type);
			gen_print(type);
		} else {
			abort_c(ERR_EXPECTED_EXPRESSION_OR_STRING);
		}
	}

	expect(TOK_RPAREN);
	DBG_end("</output>");
}

/**
 * Parses a return statement in the defined format.
 */
void parse_return(void)
{
	ValType type;
	DBG_start("<return>");

	expect(TOK_RETURN);
	if (STARTS_EXPR(token.type))
	{
		parse_expr(&type);
	}

	gen_1(JVM_RETURN);
	DBG_end("</return>");
}

/**
 * Parses a while loop in the defined format.
 */
void parse_while(void)
{
	Label	start, stop;
	ValType type;
	start = get_label();
	stop  = get_label();

	gen_label(start);

	DBG_start("<while>");
	expect(TOK_WHILE);
	parse_expr(&type);

	gen_2_label(JVM_IFEQ, stop);
	expect(TOK_COLON);
	parse_statements();

	gen_2_label(JVM_GOTO, start);
	gen_label(stop);
	expect(TOK_END);
	DBG_end("</while>");
}

/**
 *
 * Parses a list of arguments enclosed within parentheses.
 *
 */

void parse_arglist()
{
	DBG_start("<arglist>");
	ValType type;

	expect(TOK_LPAREN);

	parse_expr(&type);

	while (token.type == TOK_COMMA)
	{
		get_token(&token);
		parse_expr(&type);
	}

	expect(TOK_RPAREN);
	DBG_end("</arglist>");
}

/**
 * Parses an index.
 *
 * Handles the parsing of an index enclosed within square brackets.
 *
 */
void parse_index()
{
	DBG_start("<index>");
	ValType type;

	expect(TOK_LBRACK);

	parse_simple(&type);

	expect(TOK_RBRACK);
	DBG_end("</index>");
}

/**
 * Parses an expression.
 *
 * Parses a simple expression and optionally handles a relational operator
 * followed by another simple expression.
 *
 */
void parse_expr(ValType *type)
{
	DBG_start("<expr>");
	ValType temp;

	parse_simple(type);

	if (IS_RELOP(token.type))
	{
		switch (token.type)
		{
			case TOK_EQ:
				get_token(&token);
				parse_simple(&temp);
				gen_cmp(JVM_IF_ICMPEQ);
				break;
			case TOK_GE:
				get_token(&token);
				parse_simple(&temp);
				gen_cmp(JVM_IF_ICMPGE);
				break;
			case TOK_GT:
				get_token(&token);
				parse_simple(&temp);
				gen_cmp(JVM_IF_ICMPGT);
				break;
			case TOK_LE:
				get_token(&token);
				parse_simple(&temp);
				gen_cmp(JVM_IF_ICMPLE);
				break;
			case TOK_LT:
				get_token(&token);
				parse_simple(&temp);
				gen_cmp(JVM_IF_ICMPLT);
				break;
			case TOK_NE:
				get_token(&token);
				parse_simple(&temp);
				gen_cmp(JVM_IF_ICMPNE);
				break;
			default:
				break;
		}

		*type = TYPE_BOOLEAN;
	}

	DBG_end("</expr>");
}

/**
 * Parses a simple expression.
 *
 * Parses a simple expression that consists of terms possibly connected by
 * addition or subtraction operators. The expression may start with a minus
 * sign.
 */

void parse_simple(ValType *type)
{
	DBG_start("<simple>");

	if (token.type == TOK_MINUS)
	{

		get_token(&token);
		parse_term(type);
		gen_1(JVM_INEG);

	} else {
		parse_term(type);
	}
	while (IS_ADDOP(token.type))
	{
		switch (token.type)
		{
			case TOK_MINUS:
				get_token(&token);
				parse_term(type);
				gen_1(JVM_ISUB);
				break;
			case TOK_PLUS:
				get_token(&token);
				parse_term(type);
				gen_1(JVM_IADD);
				break;
			case TOK_OR:
				get_token(&token);
				parse_term(type);
				gen_1(JVM_IOR);
				break;
			default:
				break;
		}
	}
	DBG_end("</simple>");
}

/**
 *
 * Parses a term composed of factors possibly connected by multiplication,
 * division, or modulo operators.
 */
void parse_term(ValType *type)
{
	DBG_start("<term>");

	parse_factor(type);

	while (IS_MULOP(token.type))
	{
		switch (token.type)
		{
			case TOK_MUL:
				get_token(&token);
				parse_factor(type);
				gen_1(JVM_IMUL);
				break;
			case TOK_DIV:
				get_token(&token);
				parse_factor(type);
				gen_1(JVM_IDIV);
				break;
			case TOK_AND:
				get_token(&token);
				parse_factor(type);
				gen_1(JVM_IAND);
				break;
			case TOK_REM:
				get_token(&token);
				parse_factor(type);
				gen_1(JVM_IREM);
				break;
			default:
				break;
		}
	}
	DBG_end("</term>");
}

/**
 * Parses a factor, which can be an identifier, a number, an expression enclosed
 * in parentheses, a NOT operation, or boolean values TRUE or FALSE.
 */
void parse_factor(ValType *vt)
{
	char	*id;
	IDPropt *propt;
	DBG_start("<factor>");
	switch (token.type)
	{
		case TOK_ID:
			expect_id(&id);

			find_name(id, &propt);

			*vt = propt->type;

			if (token.type == TOK_LBRACK)
			{
				SET_BASE_TYPE(*vt);
				gen_2(JVM_ALOAD, propt->offset);
				parse_index();
				gen_1(JVM_IALOAD);
			} else if (token.type == TOK_LPAREN) {

				parse_arglist();
				gen_call(id, propt);
			} else if (IS_ARRAY_TYPE(propt->type)) {
				gen_2(JVM_ALOAD, propt->offset);
			} else {
				gen_2(JVM_ILOAD, propt->offset);
			}
			break;

		case TOK_NUM:

			*vt = TYPE_INTEGER;
			gen_2(JVM_LDC, token.value);
			get_token(&token);

			break;

		case TOK_LPAREN:
			get_token(&token);
			parse_expr(vt);
			expect(TOK_RPAREN);
			break;

		case TOK_NOT:

			get_token(&token);

			parse_factor(vt);
			gen_2(JVM_LDC, 1);
			gen_1(JVM_IXOR);

			break;

		case TOK_TRUE:

			gen_2(JVM_LDC, 1);
			*vt = TYPE_BOOLEAN;
			get_token(&token);
			break;

		case TOK_FALSE:

			gen_2(JVM_LDC, 0);

			*vt = TYPE_BOOLEAN;
			get_token(&token);
			break;

		default:
			abort_c(ERR_EXPECTED_FACTOR);
			break;
	}
	DBG_end("</factor>");
}

/* --- helper routines ------------------------------------------------------ */

#define MAX_MSG_LEN 256

/* TODO: Uncomment the following function for use during type checking. */

void chktypes(ValType found, ValType expected, SourcePos *pos, ...)
{
	char	buf[MAX_MSG_LEN], *s;
	va_list ap;

	if (found != expected)
	{
		buf[0] = '\0';
		va_start(ap, pos);
		s = va_arg(ap, char *);
		vsnprintf(buf, MAX_MSG_LEN, s, ap);
		va_end(ap);
		if (pos)
		{
			position = *pos;
		}
		leprintf("incompatible types (expected %s, found %s) %s",
			get_valtype_string(expected),
			get_valtype_string(found),
			buf);
	}
}

void expect(TokenType type)
{
	if (token.type == type)
	{
		get_token(&token);
	} else {
		abort_c(ERR_EXPECT, type);
	}
}

void expect_id(char **id)
{
	if (token.type == TOK_ID)
	{
		*id = estrdup(token.lexeme);
		get_token(&token);
	} else {

		abort_c(ERR_EXPECT, TOK_ID);
		eprintf("df");
	}
}

/* --- constructors --------------------------------------------------------- */

/* TODO: Uncomment the following two functions for use during type checking. */

IDPropt *idpropt(ValType type, unsigned int offset, unsigned int nparams,
ValType*params)
{
	IDPropt *ip = emalloc(sizeof(*ip));

	ip->type	= type;
	ip->offset	= offset;
	ip->nparams = nparams;
	ip->params	= params;

	return ip;
}

Variable *variable(char *id, ValType type, SourcePos pos)
{
	Variable *vp = emalloc(sizeof(*vp));

	vp->id	 = id;
	vp->type = type;
	vp->pos	 = pos;
	vp->next = NULL;

	return vp;
}

/* --- error handling routines ---------------------------------------------- */

void _abort_cp(SourcePos *posp, Error err, va_list args)
{
	char expstr[MAX_MSG_LEN], *s;
	int	 t;

	if (posp)
	{
		position = *posp;
	}

	snprintf(expstr,
		MAX_MSG_LEN,
		"expected %%s, but found %s",
		get_token_string(token.type));

	switch (err)
	{
		case ERR_EXPECTED_SCALAR:
		case ERR_ILLEGAL_ARRAY_OPERATION:
		case ERR_MULTIPLE_DEFINITION:
		case ERR_NOT_AN_ARRAY:
		case ERR_NOT_A_FUNCTION:
		case ERR_NOT_A_PROCEDURE:
		case ERR_NOT_A_VARIABLE:
		case ERR_TOO_FEW_ARGUMENTS:
		case ERR_TOO_MANY_ARGUMENTS:
		case ERR_UNKNOWN_IDENTIFIER:
		case ERR_UNREACHABLE:
			s = va_arg(args, char *);
			break;
		default:
			break;
	}

	switch (err)
	{
		case ERR_NOT_AN_ARRAY:

			leprintf("'%s' is not an array", s);
			break;

		case ERR_EXPECT:
			t = va_arg(args, int);
			leprintf(expstr, get_token_string(t));
			break;

		case ERR_EXPECTED_FACTOR:
			leprintf(expstr, "factor");
			break;

		case ERR_UNREACHABLE:
			leprintf("unreachable: %s", s);
			break;

		case ERR_EXPECTED_TYPE_SPECIFIER:
			leprintf("expected type specifier, but found %s",
				get_token_string(token.type));
			break;

		case ERR_EXPECTED_STATEMENT:
			leprintf("expected statement, but found %s",
				get_token_string(token.type));
			break;

		case ERR_EXPECTED_EXPRESSION_OR_ARRAY_ALLOCATION:
			leprintf("expected expression or array allocation, but found %s",
				get_token_string(token.type));
			break;

		case ERR_EXPECTED_EXPRESSION_OR_STRING:
			leprintf("expected expression or string, but found %s",
				get_token_string(token.type));
			break;

		default:
			leprintf("unreachable: %s", s);
			break;
	}
}

void abort_c(Error err, ...)
{
	va_list args;

	va_start(args, err);
	_abort_cp(NULL, err, args);
	va_end(args);
}

void abort_cp(SourcePos *posp, Error err, ...)
{
	va_list args;

	va_start(args, err);
	_abort_cp(posp, err, args);
	va_end(args);
}

/* --- debugging output routines -------------------------------------------- */

#ifdef DEBUG_PARSER

static int indent = 0;

void _debug_info(const char *fmt, va_list args)
{
	int	 i;
	char buf[MAX_MSG_LEN], *buf_ptr;

	buf_ptr = buf;

	for (i = 0; i < indent; i++)
	{
		*buf_ptr++ = ' ';
	}

	vsprintf(buf_ptr, fmt, args);
	buf_ptr += strlen(buf_ptr);
	snprintf(buf_ptr, MAX_MSG_LEN, " at %d:%d.\n", position.line, position.col);
	fflush(stdout);
	fputs(buf, stdout);
	fflush(NULL);
}

void debug_start(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	_debug_info(fmt, args);
	va_end(args);
	indent += 2;
}

void debug_end(const char *fmt, ...)
{
	va_list args;

	indent -= 2;
	va_start(args, fmt);
	_debug_info(fmt, args);
	va_end(args);
}

void debug_info(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	_debug_info(fmt, args);
	va_end(args);
}

#endif /* DEBUG_PARSER */
