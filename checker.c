/*
 * Checker
 */

#include <adt/list.h>
#include <assert.h>
#include <ast.h>
#include <checker.h>
#include <lexer.h>
#include <merrno.h>
#include <parser.h>
#include <stdbool.h>
#include <stdlib.h>

static void checker_parser_get_tok(void *, lexer_tok_t *);
static void *checker_parser_tok_data(void *, lexer_tok_t *);

static parser_input_ops_t checker_parser_input = {
	.get_tok = checker_parser_get_tok,
	.tok_data = checker_parser_tok_data
};

/** Create checker module.
 *
 * @param input_ops Input ops
 * @param input_arg Argument to input_ops
 * @param rmodule Place to store new checker module.
 *
 * @return EOK on success, ENOMEM if out of memory
 */
static int checker_module_create(checker_module_t **rmodule)
{
	checker_module_t *module = NULL;

	module = calloc(1, sizeof(checker_module_t));
	if (module == NULL)
		return ENOMEM;

	list_initialize(&module->toks);

	*rmodule = module;
	return EOK;
}

/** Destroy checker module.
 *
 * @param checker Checker
 */
static void checker_module_destroy(checker_module_t *module)
{
	free(module);
}

/** Append a token to checker module.
 *
 * @param module Checker module
 * @param tok Lexer token
 *
 * @return EOK on success, ENOMEM if out of memory
 */
static int checker_module_append(checker_module_t *module, lexer_tok_t *tok)
{
	checker_tok_t *ctok;

	ctok = calloc(1, sizeof(checker_tok_t));
	if (ctok == NULL)
		return ENOMEM;

	ctok->mod = module;
	ctok->tok = *tok;
	list_append(&ctok->ltoks, &module->toks);
	return EOK;
}

/** Create checker.
 *
 * @param input_ops Input ops
 * @param input_arg Argument to input_ops
 * @param rchecker Place to store new checker.
 *
 * @return EOK on success, ENOMEM if out of memory
 */
int checker_create(lexer_input_ops_t *input_ops, void *input_arg,
    checker_t **rchecker)
{
	checker_t *checker = NULL;
	lexer_t *lexer = NULL;
	int rc;

	checker = calloc(1, sizeof(checker_t));
	if (checker == NULL) {
		rc = ENOMEM;
		goto error;
	}

	rc = lexer_create(input_ops, input_arg, &lexer);
	if (rc != EOK) {
		assert(rc == ENOMEM);
		goto error;
	}

	checker->lexer = lexer;
	*rchecker = checker;
	return EOK;
error:
	if (lexer != NULL)
		lexer_destroy(lexer);
	if (checker != NULL)
		free(checker);
	return rc;
}

/** Destroy checker.
 *
 * @param checker Checker
 */
void checker_destroy(checker_t *checker)
{
	checker_module_destroy(checker->mod);
	lexer_destroy(checker->lexer);
	free(checker);
}

/** Lex a module.
 *
 * @param checker Checker
 * @param rmodule Place to store pointer to new module
 * @return EOK on success, or error code
 */
static int checker_module_lex(checker_t *checker, checker_module_t **rmodule)
{
	checker_module_t *module = NULL;
	bool done;
	lexer_tok_t tok;
	int rc;

	rc = checker_module_create(&module);
	if (rc != EOK) {
		assert(rc == ENOMEM);
		goto error;
	}

	done = false;
	while (!done) {
		rc = lexer_get_tok(checker->lexer, &tok);
		if (rc != EOK)
			return rc;

		rc = checker_module_append(module, &tok);
		if (rc != EOK) {
			lexer_free_tok(&tok);
			return rc;
		}

		if (tok.ttype == ltt_eof)
			done = true;
	}

	*rmodule = module;
	return EOK;
error:
	return rc;
}

/** Get first token in a checker module.
 *
 * @param mod Checker module
 * @return First token or @c NULL if the list is empty
 */
static checker_tok_t *checker_module_first_tok(checker_module_t *mod)
{
	link_t *link;

	link = list_first(&mod->toks);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, checker_tok_t, ltoks);
}

/** Get next token in a checker module
 *
 * @param tok Current token or @c NULL
 * @return Next token or @c NULL
 */
static checker_tok_t *checker_module_next_tok(checker_tok_t *tok)
{
	link_t *link;

	if (tok == NULL)
		return NULL;

	link = list_next(&tok->ltoks, &tok->mod->toks);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, checker_tok_t, ltoks);
}

/** Get previous token in a checker module
 *
 * @param tok Current token or @c NULL
 * @return Previous token or @c NULL
 */
static checker_tok_t *checker_module_prev_tok(checker_tok_t *tok)
{
	link_t *link;

	if (tok == NULL)
		return NULL;

	link = list_prev(&tok->ltoks, &tok->mod->toks);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, checker_tok_t, ltoks);
}

/** Check that a token is at the beginning of the line.
 *
 * The token must be at the beginning of the line and indented appropriately
 */
static void checker_module_check_lbegin(checker_tok_t *tok, const char *msg)
{
	checker_tok_t *p;

	p = checker_module_prev_tok(tok);
	assert(p != NULL);

	if (p->tok.ttype != ltt_wspace) {
		lexer_dprint_tok(&tok->tok, stdout);
		printf(": %s\n", msg);
	}
}

/** Non-whitespace.
 *
 * There should be non-whitespace before the token
 */
static void checker_module_check_nows_before(checker_tok_t *tok,
    const char *msg)
{
	checker_tok_t *p;

	p = checker_module_prev_tok(tok);
	assert(p != NULL);

	if (p->tok.ttype == ltt_wspace) {
		lexer_dprint_tok(&p->tok, stdout);
		printf(": %s\n", msg);
	}
}

/** Non-spacing break.
 *
 * There should be either non-whitespace or a line break before the token. */
#if 0
static void checker_module_check_nsbrk_before(checker_tok_t *tok,
    const char *msg)
{
	(void)tok; (void)msg;
}
#endif

/** Breakable space.
 *
 * There should be either a single space or a line break before the token.
 * If there is a line break, the token must be indented appropriately.
 */
#if 0
static void checker_module_check_brkspace_before(checker_tok_t *tok,
    const char *msg)
{
	(void)tok; (void)msg;
}
#endif

/** Parse a module.
 *
 * @param mod Checker module
 * @return EOK on success or error code
 */
static int checker_module_parse(checker_module_t *mod)
{
	ast_module_t *amod;
	parser_t *parser;
	checker_parser_input_t pinput;
	int rc;

	pinput.tok = checker_module_first_tok(mod);
	rc = parser_create(&checker_parser_input, &pinput, &parser);
	if (rc != EOK)
		return rc;

	rc = parser_process_module(parser, &amod);
	if (rc != EOK)
		return rc;


	if (0) {
		putchar('\n');
		rc = ast_tree_print(&amod->node, stdout);
		if (rc != EOK)
			return rc;
		putchar('\n');
	}


	mod->ast = amod;
	parser_destroy(parser);

	return EOK;
}

/** Run checks on a statement.
 *
 * @param mod Checker module
 * @param stmt AST statement
 * @return EOK on success or error code
 */
static int checker_module_check_stmt(checker_module_t *mod, ast_node_t *stmt)
{
	checker_tok_t *treturn;
	checker_tok_t *tscolon;
	ast_return_t *areturn;

	(void)mod;

	assert(stmt->ntype == ant_return);
	areturn = (ast_return_t *)stmt->ext;
	treturn = (checker_tok_t *)areturn->treturn.data;
	tscolon = (checker_tok_t *)areturn->tscolon.data;

	checker_module_check_lbegin(treturn,
	    "Statement must start on a new line.");
	checker_module_check_nows_before(tscolon,
	    "Unexpected whitespace before ';'.");

	return EOK;
}

/** Run checks on a declaration.
 *
 * @param mod Checker module
 * @param decl AST declaration
 * @return EOK on success or error code
 */
static int checker_module_check_decl(checker_module_t *mod, ast_node_t *decl)
{
	int rc;
	ast_node_t *stmt;
	ast_fundef_t *fundef;

	(void)mod;

	if (0) printf("Check function declaration\n");
	assert(decl->ntype == ant_fundef);
	fundef = (ast_fundef_t *)decl->ext;

	if (fundef->body == NULL)
		return EOK;

	stmt = ast_block_first(fundef->body);
	while (stmt != NULL) {
		rc = checker_module_check_stmt(mod, stmt);
		if (rc != EOK)
			return rc;

		stmt = ast_block_next(stmt);
	}

	return EOK;
}

/** Run checks on a module.
 *
 * @param mod Checker module
 * @return EOK on success or error code
 */
static int checker_module_check(checker_module_t *mod)
{
	int rc;
	ast_node_t *decl;

	if (0) printf("Check module\n");
	decl = ast_module_first(mod->ast);
	while (decl != NULL) {
		rc = checker_module_check_decl(mod, decl);
		if (rc != EOK)
			return rc;
		decl = ast_module_next(decl);
	}

	return EOK;
}

/** Run checker.
 *
 * @param checker Checker
 */
int checker_run(checker_t *checker)
{
	int rc;

	if (checker->mod == NULL) {
		rc = checker_module_lex(checker, &checker->mod);
		if (rc != EOK)
			return rc;

		rc = checker_module_parse(checker->mod);
		if (rc != EOK)
			return rc;

		rc = checker_module_check(checker->mod);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

/** Parser input from checker token list.
 *
 * @param arg Checker parser input (checker_parser_input_t)
 * @param tok Place to store token
 */
static void checker_parser_get_tok(void *arg, lexer_tok_t *tok)
{
	checker_parser_input_t *pinput = (checker_parser_input_t *)arg;

	*tok = pinput->tok->tok;
	/* Pass pointer to checker token down to checker_parser_tok_data */
	tok->udata = pinput->tok;

	if (tok->ttype != ltt_eof)
		pinput->tok = checker_module_next_tok(pinput->tok);
}

/** Get user data for a token.
 *
 * Return a pointer to the token. We can do this since we keep the
 * tokens in memory all the time.
 *
 * @param arg Checker parser input (checker_parser_input_t)
 * @param tok Place to store token
 */
static void *checker_parser_tok_data(void *arg, lexer_tok_t *tok)
{
	checker_tok_t *ctok;

	(void)arg;

	/* Pointer to checker_tok_t sent by checker_parser_get_tok. */
	ctok = (checker_tok_t *)tok->udata;

	/* Set this as user data for the AST token */
	return ctok;
}
