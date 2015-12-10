#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/mlrutil.h"
#include "lib/string_builder.h"
#include "containers/lrec.h"

#define SB_ALLOC_LENGTH 256

static lrece_t* lrec_find_entry(lrec_t* prec, char* key);
static void lrec_unlink(lrec_t* prec, lrece_t* pe);
static void lrec_link_at_head(lrec_t* prec, lrece_t* pe);
static void lrec_link_at_tail(lrec_t* prec, lrece_t* pe);

static void lrec_unbacked_free(lrec_t* prec);
static void lrec_free_single_line_backing(lrec_t* prec);
static void lrec_free_csv_backing(lrec_t* prec);
static void lrec_free_multiline_backing(lrec_t* prec);

// ----------------------------------------------------------------
lrec_t* lrec_unbacked_alloc() {
	lrec_t* prec = mlr_malloc_or_die(sizeof(lrec_t));
	memset(prec, 0, sizeof(lrec_t));
	prec->pfree_backing_func = lrec_unbacked_free;
	return prec;
}

lrec_t* lrec_dkvp_alloc(char* line) {
	lrec_t* prec = mlr_malloc_or_die(sizeof(lrec_t));
	memset(prec, 0, sizeof(lrec_t));
	prec->psingle_line = line;
	prec->pfree_backing_func = lrec_free_single_line_backing;
	return prec;
}

lrec_t* lrec_nidx_alloc(char* line) {
	lrec_t* prec = mlr_malloc_or_die(sizeof(lrec_t));
	memset(prec, 0, sizeof(lrec_t));
	prec->psingle_line  = line;
	prec->pfree_backing_func = lrec_free_single_line_backing;
	return prec;
}

lrec_t* lrec_csvlite_alloc(char* data_line) {
	lrec_t* prec = mlr_malloc_or_die(sizeof(lrec_t));
	memset(prec, 0, sizeof(lrec_t));
	prec->psingle_line = data_line;
	prec->pfree_backing_func = lrec_free_csv_backing;
	return prec;
}

lrec_t* lrec_csv_alloc(char* data_line) {
	lrec_t* prec = mlr_malloc_or_die(sizeof(lrec_t));
	memset(prec, 0, sizeof(lrec_t));
	prec->psingle_line = data_line;
	prec->pfree_backing_func = lrec_free_csv_backing;
	return prec;
}

lrec_t* lrec_xtab_alloc(slls_t* pxtab_lines) {
	lrec_t* prec = mlr_malloc_or_die(sizeof(lrec_t));
	memset(prec, 0, sizeof(lrec_t));
	prec->pxtab_lines = pxtab_lines;
	prec->pfree_backing_func = lrec_free_multiline_backing;
	return prec;
}

// ----------------------------------------------------------------
void lrec_put(lrec_t* prec, char* key, char* value, char free_flags) {
	lrece_t* pe = lrec_find_entry(prec, key);

	if (pe != NULL) {
		if (pe->free_flags & FREE_ENTRY_VALUE) {
			free(pe->value);
		}
		pe->value = value;
		pe->free_flags &= ~FREE_ENTRY_VALUE;
		if (free_flags & FREE_ENTRY_VALUE)
			pe->free_flags |= FREE_ENTRY_VALUE;
	} else {
		pe = mlr_malloc_or_die(sizeof(lrece_t));
		pe->key        = key;
		pe->value      = value;
		pe->free_flags = free_flags;

		if (prec->phead == NULL) {
			pe->pprev   = NULL;
			pe->pnext   = NULL;
			prec->phead = pe;
			prec->ptail = pe;
		} else {
			pe->pprev   = prec->ptail;
			pe->pnext   = NULL;
			prec->ptail->pnext = pe;
			prec->ptail = pe;
		}
		prec->field_count++;
	}
}

void lrec_put_get_rid_of(lrec_t* prec, char* key, char* value, char free_flags) {
	lrece_t* pe = lrec_find_entry(prec, key);

	if (pe != NULL) {
		if (pe->free_flags & FREE_ENTRY_VALUE) {
			free(pe->value);
		}
		pe->value = mlr_strdup_or_die(value);
		pe->free_flags |= FREE_ENTRY_VALUE;
	} else {
		pe = mlr_malloc_or_die(sizeof(lrece_t));
		pe->key         = mlr_strdup_or_die(key);
		pe->value       = mlr_strdup_or_die(value);
		pe->free_flags  = FREE_ENTRY_KEY | FREE_ENTRY_VALUE;

		if (prec->phead == NULL) {
			pe->pprev   = NULL;
			pe->pnext   = NULL;
			prec->phead = pe;
			prec->ptail = pe;
		} else {
			pe->pprev   = prec->ptail;
			pe->pnext   = NULL;
			prec->ptail->pnext = pe;
			prec->ptail = pe;
		}
		prec->field_count++;
	}
}

void lrec_prepend(lrec_t* prec, char* key, char* value, char free_flags) {
	lrece_t* pe = lrec_find_entry(prec, key);

	if (pe != NULL) {
		if (pe->free_flags & FREE_ENTRY_VALUE) {
			free(pe->value);
		}
		pe->value = mlr_strdup_or_die(value);
		pe->free_flags |= FREE_ENTRY_VALUE;
	} else {
		pe = mlr_malloc_or_die(sizeof(lrece_t));
		pe->key         = mlr_strdup_or_die(key);
		pe->value       = mlr_strdup_or_die(value);
		pe->free_flags  = FREE_ENTRY_KEY | FREE_ENTRY_VALUE;

		if (prec->phead == NULL) {
			pe->pprev   = NULL;
			pe->pnext   = NULL;
			prec->phead = pe;
			prec->ptail = pe;
		} else {
			pe->pnext   = prec->phead;
			pe->pprev   = NULL;
			prec->phead->pprev = pe;
			prec->phead = pe;
		}
		prec->field_count++;
	}
}

// ----------------------------------------------------------------
char* lrec_get(lrec_t* prec, char* key) {
	lrece_t* pe = lrec_find_entry(prec, key);
	if (pe != NULL) {
		return pe->value;
	} else {
		return NULL;
	}
}

// ----------------------------------------------------------------
void lrec_remove(lrec_t* prec, char* key) {
	lrece_t* pe = lrec_find_entry(prec, key);
	if (pe == NULL)
		return;

	lrec_unlink(prec, pe);

	if (pe->free_flags & FREE_ENTRY_KEY) {
		free(pe->key);
	}
	if (pe->free_flags & FREE_ENTRY_VALUE) {
		free(pe->value);
	}

	free(pe);
}

// Before:
//   "x" => "3"
//   "y" => "4"  <-- pold
//   "z" => "5"  <-- pnew
//
// Rename y to z
//
// After:
//   "x" => "3"
//   "z" => "4"
//
void lrec_rename(lrec_t* prec, char* old_key, char* new_key, int new_needs_freeing) {

	lrece_t* pold = lrec_find_entry(prec, old_key);
	if (pold != NULL) {
		lrece_t* pnew = lrec_find_entry(prec, new_key);

		if (pnew == NULL) { // E.g. rename "x" to "y" when "y" is not present
			if (pold->free_flags & FREE_ENTRY_KEY) {
				free(pold->key);
				pold->key = new_key;
				if (!new_needs_freeing)
					pold->free_flags &= ~FREE_ENTRY_KEY;
			} else {
				pold->key = new_key;
				if (new_needs_freeing)
					pold->free_flags |=  FREE_ENTRY_KEY;
			}

		} else { // E.g. rename "x" to "y" when "y" is already present
			if (pnew->free_flags & FREE_ENTRY_VALUE) {
				free(pnew->value);
			}
			if (pold->free_flags & FREE_ENTRY_KEY) {
				free(pold->key);
				pold->free_flags &= ~FREE_ENTRY_KEY;
			}
			pold->key = new_key;
			if (new_needs_freeing)
				pold->free_flags |=  FREE_ENTRY_KEY;
			else
				pold->free_flags &= ~FREE_ENTRY_KEY;
			lrec_unlink(prec, pnew);
			free(pnew);
		}
	}
}

// ----------------------------------------------------------------
void lrec_move_to_head(lrec_t* prec, char* key) {
	lrece_t* pe = lrec_find_entry(prec, key);
	if (pe == NULL)
		return;

	lrec_unlink(prec, pe);
	lrec_link_at_head(prec, pe);
}

void lrec_move_to_tail(lrec_t* prec, char* key) {
	lrece_t* pe = lrec_find_entry(prec, key);
	if (pe == NULL)
		return;

	lrec_unlink(prec, pe);
	lrec_link_at_tail(prec, pe);
}

// ----------------------------------------------------------------
static void lrec_unlink(lrec_t* prec, lrece_t* pe) {
	if (pe == prec->phead) {
		if (pe == prec->ptail) {
			prec->phead = NULL;
			prec->ptail = NULL;
		} else {
			prec->phead = pe->pnext;
			pe->pnext->pprev = NULL;
		}
	} else {
		pe->pprev->pnext = pe->pnext;
		if (pe == prec->ptail) {
			prec->ptail = pe->pprev;
		} else {
			pe->pnext->pprev = pe->pprev;
		}
	}
	prec->field_count--;
}

static void lrec_link_at_head(lrec_t* prec, lrece_t* pe) {

	if (prec->phead == NULL) {
		pe->pprev   = NULL;
		pe->pnext   = NULL;
		prec->phead = pe;
		prec->ptail = pe;
	} else {
		// [b,c,d] + a
		pe->pprev   = NULL;
		pe->pnext   = prec->phead;
		prec->phead->pprev = pe;
		prec->phead = pe;
	}
	prec->field_count++;
}

static void lrec_link_at_tail(lrec_t* prec, lrece_t* pe) {

	if (prec->phead == NULL) {
		pe->pprev   = NULL;
		pe->pnext   = NULL;
		prec->phead = pe;
		prec->ptail = pe;
	} else {
		pe->pprev   = prec->ptail;
		pe->pnext   = NULL;
		prec->ptail->pnext = pe;
		prec->ptail = pe;
	}
	prec->field_count++;
}

// ----------------------------------------------------------------
void lrec_free(lrec_t* prec) {
	if (prec == NULL)
		return;
	for (lrece_t* pe = prec->phead; pe != NULL; /*pe = pe->pnext*/) {
		if ((pe->free_flags & FREE_ENTRY_KEY) && (pe->key != NULL))
			free(pe->key);
		if ((pe->free_flags & FREE_ENTRY_VALUE) && (pe->value != NULL))
			free(pe->value);
		lrece_t* ope = pe;
		pe = pe->pnext;
		free(ope);
	}
	prec->pfree_backing_func(prec);
	free(prec);
}

// ----------------------------------------------------------------
void lrec_dump(lrec_t* prec) {
	printf("field_count = %d\n", prec->field_count);
	printf("| phead: %16p | ptail %16p\n", prec->phead, prec->ptail);
	for (lrece_t* pe = prec->phead; pe != NULL; pe = pe->pnext) {
		const char* key_string = (pe == NULL) ? "none" :
			pe->key == NULL ? "null" :
			pe->key;
		const char* value_string = (pe == NULL) ? "none" :
			pe->value == NULL ? "null" :
			pe->value;
		printf(
		"| prev: %16p curr: %16p next: %16p | key: %12s | value: %12s |\n",
			pe->pprev, pe, pe->pnext,
			key_string, value_string);
	}
}

void lrec_dump_titled(char* msg, lrec_t* prec) {
	printf("%s:\n", msg);
	lrec_dump(prec);
	printf("\n");
}

// ----------------------------------------------------------------
static void lrec_unbacked_free(lrec_t* prec) {
}

static void lrec_free_single_line_backing(lrec_t* prec) {
	free(prec->psingle_line);
}

static void lrec_free_csv_backing(lrec_t* prec) {
	free(prec->psingle_line);
}

static void lrec_free_multiline_backing(lrec_t* prec) {
	slls_free(prec->pxtab_lines);
}

// ----------------------------------------------------------------
static char* static_nidx_keys[] = {
	"0",   "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9",
	"10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
	"20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
	"30", "31", "32", "33", "34", "35", "36", "37", "38", "39",
	"40", "41", "42", "43", "44", "45", "46", "47", "48", "49",
	"50", "51", "52", "53", "54", "55", "56", "57", "58", "59",
	"60", "61", "62", "63", "64", "65", "66", "67", "68", "69",
	"70", "71", "72", "73", "74", "75", "76", "77", "78", "79",
	"80", "81", "82", "83", "84", "85", "86", "87", "88", "89",
	"90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "100"
};

char* make_nidx_key(int idx, char* pfree_flags) {
	if ((0 <= idx) && (idx <= 100)) {
		*pfree_flags = 0;
		return static_nidx_keys[idx];
	} else {
		char buf[32];
		sprintf(buf, "%d", idx);
		*pfree_flags = FREE_ENTRY_KEY;
		return mlr_strdup_or_die(buf);
	}
}

// ================================================================

// ----------------------------------------------------------------
// Note on efficiency:
//
// I was imagining/hoping that strcmp has additional optimizations (e.g.
// hand-coded in assembly), so I don't *want* to re-implement it (i.e. I
// probably can't outperform it).
//
// But actual experiments show I get about a 1-2% performance gain doing it
// myself (on my particular system).

static lrece_t* lrec_find_entry(lrec_t* prec, char* key) {
#if 1
	for (lrece_t* pe = prec->phead; pe != NULL; pe = pe->pnext) {
		char* pa = pe->key;
		char* pb = key;
		while (*pa && *pb && (*pa == *pb)) {
			pa++;
			pb++;
		}
		if (*pa == 0 && *pb == 0)
			return pe;
	}
	return NULL;
#else
	for (lrece_t* pe = prec->phead; pe != NULL; pe = pe->pnext)
		if (streq(pe->key, key))
			return pe;
	return NULL;
#endif
}

// ----------------------------------------------------------------
lrec_t* lrec_literal_1(char* k1, char* v1) {
	lrec_t* prec = lrec_unbacked_alloc();
	lrec_put(prec, k1, v1, NO_FREE);
	return prec;
}

lrec_t* lrec_literal_2(char* k1, char* v1, char* k2, char* v2) {
	lrec_t* prec = lrec_unbacked_alloc();
	lrec_put(prec, k1, v1, NO_FREE);
	lrec_put(prec, k2, v2, NO_FREE);
	return prec;
}

lrec_t* lrec_literal_3(char* k1, char* v1, char* k2, char* v2, char* k3, char* v3) {
	lrec_t* prec = lrec_unbacked_alloc();
	lrec_put(prec, k1, v1, NO_FREE);
	lrec_put(prec, k2, v2, NO_FREE);
	lrec_put(prec, k3, v3, NO_FREE);
	return prec;
}

lrec_t* lrec_literal_4(char* k1, char* v1, char* k2, char* v2, char* k3, char* v3, char* k4, char* v4) {
	lrec_t* prec = lrec_unbacked_alloc();
	lrec_put(prec, k1, v1, NO_FREE);
	lrec_put(prec, k2, v2, NO_FREE);
	lrec_put(prec, k3, v3, NO_FREE);
	lrec_put(prec, k4, v4, NO_FREE);
	return prec;
}

void lrec_print(lrec_t* prec) {
	FILE* output_stream = stdout;
	char ors = '\n';
	char ofs = ',';
	char ops = '=';
	if (prec == NULL) {
		fputs("NULL", output_stream);
		fputc(ors, output_stream);
		return;
	}
	int nf = 0;
	for (lrece_t* pe = prec->phead; pe != NULL; pe = pe->pnext) {
		if (nf > 0)
			fputc(ofs, output_stream);
		fputs(pe->key, output_stream);
		fputc(ops, output_stream);
		fputs(pe->value, output_stream);
		nf++;
	}
	fputc(ors, output_stream);
}

char* lrec_sprint(lrec_t* prec, char* ors, char* ofs, char* ops) {
	string_builder_t* psb = sb_alloc(SB_ALLOC_LENGTH);
	if (prec == NULL) {
		sb_append_string(psb, "NULL");
	} else {
		int nf = 0;
		for (lrece_t* pe = prec->phead; pe != NULL; pe = pe->pnext) {
			if (nf > 0)
				sb_append_string(psb, ofs);
			sb_append_string(psb, pe->key);
			sb_append_string(psb, ops);
			sb_append_string(psb, pe->value);
			nf++;
		}
		sb_append_string(psb, ors);
	}
	char* rv = sb_finish(psb);
	sb_free(psb);
	return rv;
}
