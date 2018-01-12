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
 * Test AST
 */

#include <ast.h>
#include <merrno.h>
#include <stdio.h>
#include <test/ast.h>

/** Test AST module.
 *
 * @return EOK on success or non-zero error code
 */
static int test_ast_module(void)
{
	ast_module_t *module;
	ast_fundef_t *fundef;
	ast_dspecs_t *dspecs;
	ast_dident_t *fdecl;
	int rc;

	rc = ast_module_create(&module);
	if (rc != EOK)
		return rc;

	rc = ast_dspecs_create(&dspecs);
	if (rc != EOK)
		return rc;

	rc = ast_dident_create(&fdecl);
	if (rc != EOK)
		return rc;

	rc = ast_fundef_create(dspecs, &fdecl->node, NULL, &fundef);
	if (rc != EOK)
		return rc;

	ast_module_append(module, &fundef->node);

	ast_tree_print(&module->node, stdout);
	putchar('\n');

	ast_tree_destroy(&module->node);
	return EOK;
}

/** Test AST function definition.
 *
 * @return EOK on success or non-zero error code
 */
static int test_ast_fundef(void)
{
	ast_fundef_t *fundef;
	ast_dspecs_t *dspecs;
	ast_dident_t *fdecl;
	int rc;

	rc = ast_dspecs_create(&dspecs);
	if (rc != EOK)
		return rc;

	rc = ast_dident_create(&fdecl);
	if (rc != EOK)
		return rc;

	rc = ast_fundef_create(dspecs, &fdecl->node, NULL, &fundef);
	if (rc != EOK)
		return rc;

	ast_tree_print(&fundef->node, stdout);
	putchar('\n');

	ast_tree_destroy(&fundef->node);
	return EOK;
}

/** Test AST block.
 *
 * @return EOK on success or non-zero error code
 */
static int test_ast_block(void)
{
	ast_block_t *block;
	ast_return_t *areturn;
	int rc;

	rc = ast_block_create(ast_braces, &block);
	if (rc != EOK)
		return rc;

	rc = ast_return_create(&areturn);
	if (rc != EOK)
		return rc;

	ast_block_append(block, &areturn->node);

	ast_tree_print(&block->node, stdout);
	putchar('\n');

	ast_tree_destroy(&block->node);
	return EOK;
}

/** Test AST type specifier.
 *
 * @return EOK on success or non-zero error code
 */
static int test_ast_tspec(void)
{
	ast_tsbasic_t *atspec;
	int rc;

	rc = ast_tsbasic_create(&atspec);
	if (rc != EOK)
		return rc;

	ast_tree_print(&atspec->node, stdout);
	putchar('\n');

	return EOK;
}

/** Test AST return.
 *
 * @return EOK on success or non-zero error code
 */
static int test_ast_return(void)
{
	ast_return_t *areturn;
	int rc;

	rc = ast_return_create(&areturn);
	if (rc != EOK)
		return rc;

	ast_tree_print(&areturn->node, stdout);
	putchar('\n');

	return EOK;
}

/** Run AST tests.
 *
 * @return EOK on success or non-zero error code
 */
int test_ast(void)
{
	int rc;

	rc = test_ast_module();
	if (rc != EOK)
		return rc;

	rc = test_ast_fundef();
	if (rc != EOK)
		return rc;

	rc = test_ast_block();
	if (rc != EOK)
		return rc;

	rc = test_ast_tspec();
	if (rc != EOK)
		return rc;

	rc = test_ast_return();
	if (rc != EOK)
		return rc;

	return EOK;
}
