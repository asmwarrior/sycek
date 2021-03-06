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

#ifndef CHECKER_H
#define CHECKER_H

#include <stdbool.h>
#include <stdio.h>
#include <types/checker.h>
#include <types/lexer.h>

extern int checker_create(lexer_input_ops_t *, void *, checker_mtype_t,
    checker_cfg_t *, checker_t **);
extern int checker_print(checker_t *, FILE *);
extern int checker_dump_ast(checker_t *, FILE *);
extern int checker_dump_toks(checker_t *, FILE *);
extern void checker_destroy(checker_t *);
extern int checker_run(checker_t *, bool);
extern void checker_cfg_init(checker_cfg_t *);

#endif
