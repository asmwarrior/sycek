/*
 * Source code position
 */

#include <merrno.h>
#include <src_pos.h>
#include <stdio.h>

/** Print position range.
 *
 * @param bpos Position of the beginning of range
 * @param epos Position of the end of the range (inclusive)
 * @param f Output file
 *
 * @return EOK on success, EIO on I.O error
 */
int src_pos_print_range(src_pos_t *bpos, src_pos_t *epos, FILE *f)
{
	if (bpos->line == epos->line && bpos->col == epos->col) {
		if (fprintf(f, "file:%zu:%zu", bpos->line, bpos->col) < 0)
			return EIO;
	} else if (bpos->line == epos->line) {
		if (fprintf(f, "file:%zu:%zu-%zu", bpos->line, bpos->col,
		    epos->col) < 0)
			return EIO;
	} else {
		if (fprintf(f, "file:%zu:%zu-%zu:%zu", bpos->line, bpos->col,
		    epos->line, epos->col) < 0)
			return EIO;
	}

	return EOK;
}
