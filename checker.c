/*
 * Copyright 2018 Jiri Svoboda
 *
 * Permission is hereby granted, free of charge, to any person obtaining 
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

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
static int checker_check_decl(checker_scope_t *, ast_node_t *);
static int checker_check_tspec(checker_scope_t *, ast_node_t *);
static int checker_check_sqlist(checker_scope_t *, ast_sqlist_t *);

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
static checker_tok_t *checker_next_tok(checker_tok_t *tok)
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
static checker_tok_t *checker_prev_tok(checker_tok_t *tok)
{
	link_t *link;

	if (tok == NULL)
		return NULL;

	link = list_prev(&tok->ltoks, &tok->mod->toks);
	if (link == NULL)
		return NULL;

	return list_get_instance(link, checker_tok_t, ltoks);
}

/** Check a token that does not itself have whitespace requirements.
 *
 * @param scope Checker scope
 * @param tok Token
 */
static void checker_check_any(checker_scope_t *scope, checker_tok_t *tok)
{
	tok->indlvl = scope->indlvl;
}

/** Check a token that must be at the beginning of the line.
 *
 * The token must be at the beginning of the line and indented appropriately
 */
static void checker_check_lbegin(checker_scope_t *scope, checker_tok_t *tok,
    const char *msg)
{
	checker_tok_t *p;

	checker_check_any(scope, tok);

	p = checker_prev_tok(tok);
	assert(p != NULL);

	if (p->tok.ttype != ltt_wspace) {
		lexer_dprint_tok(&tok->tok, stdout);
		printf(": %s\n", msg);
	}
}

/** Check no whitespace before.
 *
 * There should be non-whitespace before the token
 */
static void checker_check_nows_before(checker_scope_t *scope,
    checker_tok_t *tok, const char *msg)
{
	checker_tok_t *p;

	checker_check_any(scope, tok);

	assert(tok != NULL);
	p = checker_prev_tok(tok);
	assert(p != NULL);

	if (p->tok.ttype == ltt_wspace) {
		lexer_dprint_tok(&p->tok, stdout);
		printf(": %s\n", msg);
	}
}

/** Check no whitespace after.
 *
 * There should be non-whitespace after the token
 */
static void checker_check_nows_after(checker_scope_t *scope,
    checker_tok_t *tok, const char *msg)
{
	checker_tok_t *p;

	checker_check_any(scope, tok);

	assert(tok != NULL);
	p = checker_next_tok(tok);
	assert(p != NULL);

	if (p->tok.ttype == ltt_wspace) {
		lexer_dprint_tok(&p->tok, stdout);
		printf(": %s\n", msg);
	}
}

/** Check non-spacing break bfore.
 *
 * There should be either non-whitespace or a line break before the token. */
#if 0
static void checker_check_nsbrk_before(checker_scope_t *scope,
    checker_tok_t *tok, const char *msg)
{
	checker_ckeck_any(scope, tok);
	(void)msg;
}
#endif

/** Breakable space.
 *
 * There should be either a single space or a line break before the token.
 * If there is a line break, the token must be indented appropriately.
 */
#if 0
static void checker_check_brkspace_before(checker_scope_t *scope,
    checker_tok_t *tok, const char *msg)
{
	checker_check_any(scope, tok);
	(void)msg;
}
#endif

/** Check non-breakable space before.
 *
 * There should be a single space before the token
 */
static void checker_check_nbspace_before(checker_scope_t *scope,
    checker_tok_t *tok, const char *msg)
{
	checker_tok_t *p;

	checker_check_any(scope, tok);

	assert(tok != NULL);
	p = checker_prev_tok(tok);
	assert(p != NULL);

	if (p->tok.ttype != ltt_wspace) {
		lexer_dprint_tok(&p->tok, stdout);
		printf(": %s\n", msg);
	}
}

/** Create top-level checker scope.
 *
 * @param mod Checker module
 * @return New scope or @c NULL if out of memory
 */
static checker_scope_t *checker_scope_toplvl(checker_module_t *mod)
{
	checker_scope_t *tscope;

	tscope = calloc(1, sizeof(checker_scope_t));
	if (tscope == NULL)
		return NULL;

	tscope->mod = mod;
	tscope->indlvl = 0;

	return tscope;
}

/** Create nested scope.
 *
 * @param scope Containing scope
 * @return New scope or @c NULL if out of memory
 */
static checker_scope_t *checker_scope_nested(checker_scope_t *scope)
{
	checker_scope_t *nscope;

	nscope = calloc(1, sizeof(checker_scope_t));
	if (nscope == NULL)
		return NULL;

	nscope->mod = scope->mod;
	nscope->indlvl = scope->indlvl + 1;

	return nscope;
}

/** Destroy scope.
 *
 * @param scope Checker scope
 */
static void checker_scope_destroy(checker_scope_t *scope)
{
	free(scope);
}

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
 * @param scope Checker scope
 * @param stmt AST statement
 * @return EOK on success or error code
 */
static int checker_check_stmt(checker_scope_t *scope, ast_node_t *stmt)
{
	checker_tok_t *treturn;
	checker_tok_t *tscolon;
	ast_return_t *areturn;

	assert(stmt->ntype == ant_return);
	areturn = (ast_return_t *)stmt->ext;
	treturn = (checker_tok_t *)areturn->treturn.data;
	tscolon = (checker_tok_t *)areturn->tscolon.data;

	checker_check_lbegin(scope, treturn,
	    "Statement must start on a new line.");
	checker_check_nows_before(scope, tscolon,
	    "Unexpected whitespace before ';'.");

	return EOK;
}

/** Run checks on an identifier declarator.
 *
 * @param scope Checker scope
 * @param dident AST identifier declarator
 * @return EOK on success or error code
 */
static int checker_check_dident(checker_scope_t *scope, ast_dident_t *dident)
{
	checker_tok_t *tident;

	tident = (checker_tok_t *)dident->tident.data;
	checker_check_any(scope, tident);
	return EOK;
}

/** Run checks on a parenthesized declarator.
 *
 * @param scope Checker scope
 * @param dparen AST parenthesized declarator
 * @return EOK on success or error code
 */
static int checker_check_dparen(checker_scope_t *scope, ast_dparen_t *dparen)
{
	checker_tok_t *tlparen;
	checker_tok_t *trparen;

	tlparen = (checker_tok_t *)dparen->tlparen.data;
	checker_check_nows_after(scope, tlparen,
	    "Unexpected whitespace after '('.");

	trparen = (checker_tok_t *)dparen->trparen.data;
	checker_check_nows_before(scope, trparen,
	    "Unexpected whitespace before ')'.");

	return checker_check_decl(scope, dparen->bdecl);
}

/** Run checks on a pointer declarator.
 *
 * @param scope Checker scope
 * @param dptr AST pointer declarator
 * @return EOK on success or error code
 */
static int checker_check_dptr(checker_scope_t *scope, ast_dptr_t *dptr)
{
	checker_tok_t *tasterisk;

	tasterisk = (checker_tok_t *)dptr->tasterisk.data;
	checker_check_nows_after(scope, tasterisk,
	    "Unexpected whitespace after '*'.");

	return checker_check_decl(scope, dptr->bdecl);
}

/** Run checks on a function declarator.
 *
 * @param scope Checker scope
 * @param dfun AST function declarator
 * @return EOK on success or error code
 */
static int checker_check_dfun(checker_scope_t *scope, ast_dfun_t *dfun)
{
	checker_tok_t *tlparen;
	ast_dfun_arg_t *arg;
	checker_tok_t *tcomma;
	checker_tok_t *trparen;
	int rc;

	rc = checker_check_decl(scope, dfun->bdecl);
	if (rc != EOK)
		return rc;

	tlparen = (checker_tok_t *)dfun->tlparen.data;
	checker_check_nows_after(scope, tlparen,
	    "Unexpected whitespace after '('.");

	arg = ast_dfun_first(dfun);
	while (arg != NULL) {
		rc = checker_check_tspec(scope, arg->tspec);
		if (rc != EOK)
			return rc;

		rc = checker_check_decl(scope, arg->decl);
		if (rc != EOK)
			return rc;

		tcomma = (checker_tok_t *)arg->tcomma.data;
		if (tcomma != NULL) {
			checker_check_nows_before(scope, tcomma,
			    "Unexpected whitespace before ','.");
		}

		arg = ast_dfun_next(arg);
	}

	trparen = (checker_tok_t *)dfun->trparen.data;
	checker_check_nows_before(scope, trparen,
	    "Unexpected whitespace before ')'.");

	return EOK;
}

/** Run checks on an array declarator.
 *
 * @param scope Checker scope
 * @param dfun AST array declarator
 * @return EOK on success or error code
 */
static int checker_check_darray(checker_scope_t *scope,
    ast_darray_t *darray)
{
	checker_tok_t *tlbracket;
	checker_tok_t *trbracket;
	int rc;

	rc = checker_check_decl(scope, darray->bdecl);
	if (rc != EOK)
		return rc;

	tlbracket = (checker_tok_t *)darray->tlbracket.data;
	checker_check_nows_after(scope, tlbracket,
	    "Unexpected whitespace after '['.");

	trbracket = (checker_tok_t *)darray->trbracket.data;
	checker_check_nows_before(scope, trbracket,
	    "Unexpected whitespace before ']'.");

	return EOK;
}

/** Run checks on a declarator.
 *
 * @param scope Checker scope
 * @param decl AST declarator
 * @return EOK on success or error code
 */
static int checker_check_decl(checker_scope_t *scope, ast_node_t *decl)
{
	int rc;

	switch (decl->ntype) {
	case ant_dnoident:
		rc = EOK;
		break;
	case ant_dident:
		rc = checker_check_dident(scope, (ast_dident_t *)decl->ext);
		break;
	case ant_dparen:
		rc = checker_check_dparen(scope, (ast_dparen_t *)decl->ext);
		break;
	case ant_dptr:
		rc = checker_check_dptr(scope, (ast_dptr_t *)decl->ext);
		break;
	case ant_dfun:
		rc = checker_check_dfun(scope, (ast_dfun_t *)decl->ext);
		break;
	case ant_darray:
		rc = checker_check_darray(scope, (ast_darray_t *)decl->ext);
		break;
	default:
		assert(false);
		rc = EOK;
		break;
	}

	return rc;
}

/** Run checks on a declarator list.
 *
 * @param scope Checker scope
 * @param dlist AST declarator list
 * @return EOK on success or error code
 */
static int checker_check_dlist(checker_scope_t *scope, ast_dlist_t *dlist)
{
	ast_dlist_entry_t *entry;
	checker_tok_t *tcomma;
	int rc;

	entry = ast_dlist_first(dlist);
	while (entry != NULL) {
		tcomma = (checker_tok_t *)entry->tcomma.data;
		if (tcomma != NULL) {
			checker_check_nows_before(scope, tcomma,
			    "Unexpected whitespace before ','.");
		}

		rc = checker_check_decl(scope, entry->decl);
		if (rc != EOK)
			return rc;

		entry = ast_dlist_next(entry);
	}

	return EOK;
}

/** Run checks on a type qualifier.
 *
 * @param scope Checker scope
 * @param tqual AST type qualifier
 * @return EOK on success or error code
 */
static int checker_check_tqual(checker_scope_t *scope, ast_tqual_t *tqual)
{
	checker_tok_t *ttqual;

	ttqual = (checker_tok_t *) tqual->tqual.data;
	checker_check_any(scope, ttqual);

	return EOK;
}

/** Run checks on a basic type specifier.
 *
 * @param scope Checker scope
 * @param tsbasic AST basic type specifier
 * @return EOK on success or error code
 */
static int checker_check_tsbasic(checker_scope_t *scope,
    ast_tsbasic_t *tsbasic)
{
	checker_tok_t *tbasic;

	tbasic = (checker_tok_t *) tsbasic->tbasic.data;
	checker_check_any(scope, tbasic);

	return EOK;
}

/** Run checks on an identifier type specifier.
 *
 * @param scope Checker scope
 * @param tsident AST identifier type specifier
 * @return EOK on success or error code
 */
static int checker_check_tsident(checker_scope_t *scope,
    ast_tsident_t *tsident)
{
	checker_tok_t *tident;

	tident = (checker_tok_t *)tsident->tident.data;
	checker_check_any(scope, tident);

	return EOK;
}

/** Run checks on a record type specifier.
 *
 * @param scope Checker scope
 * @param tsrecord AST record type specifier
 * @return EOK on success or error code
 */
static int checker_check_tsrecord(checker_scope_t *scope,
    ast_tsrecord_t *tsrecord)
{
	checker_tok_t *tlbrace;
	ast_tsrecord_elem_t *elem;
	checker_tok_t *tsu;
	checker_tok_t *tident;
	checker_tok_t *trbrace;
	checker_tok_t *tscolon;
	checker_scope_t *escope;
	int rc;

	escope = checker_scope_nested(scope);
	if (escope == NULL)
		return ENOMEM;

	tsu = (checker_tok_t *)tsrecord->tsu.data;
	checker_check_any(scope, tsu);

	tident = (checker_tok_t *)tsrecord->tident.data;
	if (tident != NULL)
		checker_check_any(scope, tident);

	tlbrace = (checker_tok_t *)tsrecord->tlbrace.data;
	if (tlbrace != NULL) {
		checker_check_nbspace_before(scope, tlbrace,
		    "Expected single space before '{'.");
	}

	elem = ast_tsrecord_first(tsrecord);
	while (elem != NULL) {
		rc = checker_check_sqlist(escope, elem->sqlist);
		if (rc != EOK)
			goto error;

		rc = checker_check_dlist(escope, elem->dlist);
		if (rc != EOK)
			goto error;

		tscolon = (checker_tok_t *)elem->tscolon.data;
		checker_check_nows_before(escope, tscolon,
		    "Unexpected whitespace before ';'.");

		elem = ast_tsrecord_next(elem);
	}

	trbrace = (checker_tok_t *)tsrecord->trbrace.data;
	if (trbrace != NULL) {
		checker_check_lbegin(scope, trbrace,
		    "'}' must begin on a new line.");
	}

	checker_scope_destroy(escope);
	return EOK;
error:
	checker_scope_destroy(escope);
	return rc;
}

/** Run checks on an enum type specifier.
 *
 * @param scope Checker scope
 * @param tsenum AST enum type specifier
 * @return EOK on success or error code
 */
static int checker_check_tsenum(checker_scope_t *scope, ast_tsenum_t *tsenum)
{
	ast_tsenum_elem_t *elem;
	checker_tok_t *tenum;
	checker_tok_t *tident;
	checker_tok_t *tlbrace;
	checker_tok_t *telem;
	checker_tok_t *tequals;
	checker_tok_t *tinit;
	checker_tok_t *tcomma;
	checker_tok_t *trbrace;
	checker_scope_t *escope;

	escope = checker_scope_nested(scope);
	if (escope == NULL)
		return ENOMEM;

	tenum = (checker_tok_t *)tsenum->tenum.data;
	checker_check_any(scope, tenum);

	tident = (checker_tok_t *)tsenum->tident.data;
	if (tident != NULL)
		checker_check_any(scope, tident);

	tlbrace = (checker_tok_t *)tsenum->tlbrace.data;
	if (tlbrace != NULL) {
		checker_check_nbspace_before(scope, tlbrace,
		    "Expected single space before '{'.");
	}

	elem = ast_tsenum_first(tsenum);
	while (elem != NULL) {
		telem = (checker_tok_t *)elem->tident.data;
		checker_check_lbegin(escope, telem,
		    "Enum field must begin on a new line.");

		tequals = (checker_tok_t *)elem->tequals.data;
		if (tequals != NULL) {
			checker_check_nbspace_before(escope, tequals,
			    "Expected space before '='.");

			tinit = (checker_tok_t *)elem->tinit.data;
			checker_check_nbspace_before(escope, tinit,
			    "Expected whitespace before initializer.");
		}

		tcomma = (checker_tok_t *)elem->tcomma.data;
		if (tcomma != NULL) {
			checker_check_nows_before(escope, tcomma,
			    "Unexpected whitespace before ','.");
		}

		elem = ast_tsenum_next(elem);
	}

	trbrace = (checker_tok_t *)tsenum->trbrace.data;
	if (trbrace != NULL) {
		checker_check_lbegin(scope, trbrace,
		    "'}' must begin on a new line.");
	}

	checker_scope_destroy(escope);
	return EOK;
}

/** Run checks on a type specifier.
 *
 * @param scope Checker scope
 * @param tspec AST type specifier
 * @return EOK on success or error code
 */
static int checker_check_tspec(checker_scope_t *scope, ast_node_t *tspec)
{
	int rc;

	switch (tspec->ntype) {
	case ant_tsbasic:
		rc = checker_check_tsbasic(scope, (ast_tsbasic_t *)tspec->ext);
		break;
	case ant_tsident:
		rc = checker_check_tsident(scope, (ast_tsident_t *)tspec->ext);
		break;
	case ant_tsrecord:
		rc = checker_check_tsrecord(scope, (ast_tsrecord_t *)tspec->ext);
		break;
	case ant_tsenum:
		rc = checker_check_tsenum(scope, (ast_tsenum_t *)tspec->ext);
		break;
	default:
		assert(false);
		rc = EOK;
		break;
	}

	return rc;
}

/** Run checks on a specifier-qualifier list.
 *
 * @param scope Checker scope
 * @param sqlist AST specifier-qualifier list
 * @return EOK on success or error code
 */
static int checker_check_sqlist(checker_scope_t *scope, ast_sqlist_t *sqlist)
{
	ast_node_t *elem;
	ast_tqual_t *tqual;
	int rc;

	elem = ast_sqlist_first(sqlist);
	while (elem != NULL) {
		if (elem->ntype == ant_tqual) {
			tqual = (ast_tqual_t *) elem->ext;
			rc = checker_check_tqual(scope, tqual);
		} else {
			rc = checker_check_tspec(scope, elem);
		}

		if (rc != EOK)
			return rc;

		elem = ast_sqlist_next(elem);
	}

	return EOK;
}


/** Run checks on a global declaration.
 *
 * @param scope Checker scope
 * @param decl AST declaration
 * @return EOK on success or error code
 */
static int checker_check_gdecln(checker_scope_t *scope, ast_node_t *decl)
{
	int rc;
	ast_node_t *stmt;
	ast_fundef_t *fundef;
	checker_tok_t *tscolon;
	checker_scope_t *bscope = NULL;

	if (0) printf("Check function declaration\n");
	assert(decl->ntype == ant_fundef);
	fundef = (ast_fundef_t *)decl->ext;

	if (fundef->body == NULL) {
		tscolon = (checker_tok_t *)fundef->tscolon.data;
		checker_check_nows_before(scope, tscolon,
		    "Unexpected whitespace before ';'.");
		return EOK;
	}

	bscope = checker_scope_nested(scope);
	if (bscope == NULL)
		return ENOMEM;

	stmt = ast_block_first(fundef->body);
	while (stmt != NULL) {
		rc = checker_check_stmt(bscope, stmt);
		if (rc != EOK)
			goto error;

		stmt = ast_block_next(stmt);
	}

	checker_scope_destroy(bscope);
	return EOK;
error:
	if (bscope != NULL)
		checker_scope_destroy(bscope);
	return rc;
}

/** Run checks on a type definition.
 *
 * @param scope Checker scope
 * @param decl AST declaration
 * @return EOK on success or error code
 */
static int checker_check_typedef(checker_scope_t *scope, ast_node_t *decln)
{
	checker_tok_t *ttypedef;
	checker_tok_t *tscolon;
	ast_typedef_t *atypedef;
	int rc;

	assert(decln->ntype == ant_typedef);
	atypedef = (ast_typedef_t *)decln->ext;

	ttypedef = (checker_tok_t *)atypedef->ttypedef.data;
	checker_check_any(scope, ttypedef);

	rc = checker_check_tspec(scope, atypedef->tspec);
	if (rc != EOK)
		return rc;

	rc = checker_check_dlist(scope, atypedef->dlist);
	if (rc != EOK)
		return rc;

	tscolon = (checker_tok_t *)atypedef->tscolon.data;
	checker_check_nows_before(scope, tscolon,
	    "Unexpected whitespace before ';'.");

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
	checker_scope_t *scope;

	if (0) printf("Check module\n");

	scope = checker_scope_toplvl(mod);
	if (scope == NULL)
		return ENOMEM;

	decl = ast_module_first(mod->ast);
	while (decl != NULL) {
		switch (decl->ntype) {
		case ant_fundef:
			rc = checker_check_gdecln(scope, decl);
			break;
		case ant_typedef:
			rc = checker_check_typedef(scope, decl);
			break;
		default:
			assert(false);
			break;
		}

		if (rc != EOK) {
			checker_scope_destroy(scope);
			return rc;
		}

		decl = ast_module_next(decl);
	}

	checker_scope_destroy(scope);
	return EOK;
}

/** Check line breaks, indentation and end-of-line whitespace.
 *
 * @param mod Checker module
 * @return EOK on success or error code
 */
static int checker_module_lines(checker_module_t *mod)
{
	checker_tok_t *ctok;

	ctok = checker_module_first_tok(mod);
	while (0 && ctok != NULL) {
		(void) lexer_dprint_tok(&ctok->tok, stdout);
		printf(" lvl %d\n", ctok->indlvl);
		ctok = checker_next_tok(ctok);
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

		rc = checker_module_lines(checker->mod);
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
		pinput->tok = checker_next_tok(pinput->tok);
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
