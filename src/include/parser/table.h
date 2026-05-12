/*
 * table.h — Stage 3: tables
 */
#pragma once
#include <stddef.h>
#include "token.h"
#include "accum.h"
#include "config.h"
#include "util/string_util.h"
#include <stdbool.h>

/**
 * Parse tables `{| ... |}` and replace them with sentinel markers while
 * pushing corresponding `TOKEN_TABLE` tokens into the accumulator.
 */
void parse_table(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum);

/* ── Table scanner helpers (callback-based, no PCRE) ─────────────────── */

/* Returns the number of leading whitespace bytes and [cno]-type sentinels. */
size_t table_lead_skip(const char *line, size_t len);

typedef struct {
	size_t      colon_count;
	const char *pre_ws;
	size_t      pre_ws_len;
	bool        opener_is_lbrace_sentinel;  /* matched \x00\d+\{\x7F */
	bool        opener_has_bang_sentinel;   /* matched { + sentinels + \x00\d+!\x7F */
	const char *opener;
	size_t      opener_len;
	const char *rest;
	size_t      rest_len;
} TableStartResult;

/* Parse a table-start line. Returns true on match and fills `out`. */
bool table_start_parse(const char *line, size_t len, TableStartResult *out);

typedef enum { TABLE_LINE_CLOSE = 0, TABLE_LINE_ROW = 1, TABLE_LINE_CELL = 2 } TableLineKind;

typedef struct {
	TableLineKind kind;
	bool          is_th;      /* cell opener was '!'         (CELL only) */
	bool          has_plus;   /* cell opener had trailing '+' (CELL only) */
	const char   *rest;
	size_t        rest_len;
} TableLineResult;

/* Classify a table line into close / row / cell. Returns false when not a
 * table-related line. On success `out->rest` points at the remainder of the
 * line after the matched opener/close. */
bool table_line_classify(const char *line, size_t len, TableLineResult *out);

typedef enum { TABLE_SEP_DOUBLE_BANG = 0, TABLE_SEP_DOUBLE_PIPE = 1, TABLE_SEP_PLUS_SENTINEL = 2 } TableSepKind;
typedef void (*TableSepCb)(TableSepKind kind, size_t pos, size_t sep_len, void *user_data);

/* Scan `buf` for cell separators in left-to-right order. `include_double_bang`
 * enables the `!!` match for th rows. */
void table_sep_scan(const char *buf, size_t len, bool include_double_bang, TableSepCb cb, void *user_data);

typedef void (*TdInnerSepCb)(bool is_sentinel, size_t pos, size_t sep_len, void *user_data);

/* Find the first '|' or \x00\d+!\x7F sentinel in `buf`. Fires cb once and
 * returns true on match. */
bool td_inner_sep_find(const char *buf, size_t len, TdInnerSepCb cb, void *user_data);
