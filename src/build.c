/*
 * build.c — Build phase implementation.
 *
 * Mirrors Token.buildFromStr() and Token.build() in dist/src/index.js:
 *
 *   buildFromStr(str) {
 *     return str.split(/[\0\x7F]/).map((s, i) => {
 *       if (i % 2 === 0) return s && new AstText(s);
 *       const n = Number(s.slice(0, -1));   // strip the type char
 *       return this.#accum[n];
 *     }).filter(Boolean);
 *   }
 *
 *   build() {
 *     const str = this.firstChild.toString();
 *     if (str.includes('\0')) {
 *       setChildNodes(this, 0, 1, this.buildFromStr(str));
 *       if (this.type === 'root') {
 *         for (const token of this.#accum) token?.build();
 *       }
 *     }
 *   }
 */
#include "build.h"
#include "token.h"
#include "accum.h"
#include "string_util.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

void build_from_str(Token *parent, const char *str, size_t str_len,
                    Accum *accum)
{
    char *src = malloc(str_len + 1);
    assert(src);
    memcpy(src, str, str_len);
    src[str_len] = '\0';

    /* Free existing children first */
    for (size_t i = 0; i < parent->child_count; i++) {
        Child *c = &parent->children[i];
        if (c->is_text) free(c->text);
        /* Token pointers are owned by the accum — do NOT free them here */
    }
    parent->child_count = 0;

    /* Walk str, splitting on \0 … \x7F markers.
     * The marker format is: \0 <decimal digits> <type_char> \x7F
     * We split at \0 and \x7F boundaries, alternating text / reference.
     * Bit-by-bit:
     *   Even segments (before first \0, between .\x7F and next \0): text nodes
     *   Odd segments (between \0 and \x7F, i.e. "N<ch>"): token reference
     */
    size_t seg_start = 0;
    bool in_marker   = false;

    for (size_t i = 0; i <= str_len; ) {
        unsigned char c = (i < str_len) ? (unsigned char)src[i] : 0;

        if (!in_marker) {
            if (c == '\0' || i == str_len) {
                /* Emit text segment [seg_start, i) */
                size_t text_len = i - seg_start;
                if (text_len > 0) {
                    token_append_text_n(parent, src + seg_start, text_len);
                }
                if (c == '\0') {
                    seg_start  = i + 1;
                    in_marker  = true;
                }
                i++;
            } else {
                i++;
            }
        } else {
            /* Inside marker — find the \x7F */
            if (c == '\x7F' || i == str_len) {
                /* Segment is "N<type_ch>" where N is decimal */
                const char *marker_content = src + seg_start;
                size_t      marker_len     = i - seg_start;

                if (marker_len >= 2) {
                    /* Parse decimal index (all but last char) */
                    size_t idx = 0;
                    bool   valid = true;
                    for (size_t d = 0; d < marker_len - 1; d++) {
                        char dc = marker_content[d];
                        if (dc >= '0' && dc <= '9') {
                            idx = idx * 10 + (size_t)(dc - '0');
                        } else {
                            valid = false;
                            break;
                        }
                    }
                    if (valid) {
                        Token *child = accum_get(accum, idx);
                        if (child) {
                            token_append_child(parent, child);
                        } else {
                            log_error("build_from_str: accum[%zu] is NULL", idx);
                        }
                    } else {
                        /* Not a valid sentinel — emit as text */
                        /* Reconstruct the raw bytes: \0 + segment + \x7F */
                        size_t raw_len = marker_len + 2;
                    char *raw = malloc(raw_len + 1);
                    assert(raw);
                    raw[0] = '\0';
                    memcpy(raw + 1, marker_content, marker_len);
                    raw[raw_len - 1] = '\x7F';
                    raw[raw_len]     = '\0';
                    token_append_text_n(parent, raw, raw_len);
                    free(raw);
                    }
                }

                seg_start = i + 1;
                in_marker = false;
                i++;
            } else {
                i++;
            }
        }
    }

    free(src);
}

/* Recursively expand sentinel markers in all text descendants of a token.
 * Mirrors JS Token.build() which calls buildFromStr on each token's firstChild
 * text when it contains a \0 sentinel. */
void build_token_recursive(Token *t, Accum *accum)
{
    if (!t) return;

    bool has_marker_text = false;
    bool all_text_children = true;
    size_t total_text_len = 0;

    for (size_t j = 0; j < t->child_count; j++) {
        Child *c = &t->children[j];
        if (!c->is_text) {
            all_text_children = false;
            continue;
        }
        total_text_len += c->text_len;
        if (c->text && memchr(c->text, '\x7F', c->text_len)) {
            has_marker_text = true;
        }
    }

    if (has_marker_text && all_text_children) {
        char *joined = malloc(total_text_len + 1);
        assert(joined);
        size_t pos = 0;
        for (size_t j = 0; j < t->child_count; j++) {
            Child *c = &t->children[j];
            memcpy(joined + pos, c->text, c->text_len);
            pos += c->text_len;
        }
        joined[total_text_len] = '\0';
        build_from_str(t, joined, total_text_len, accum);
        free(joined);
    }

    /* For each child of t: */
    for (size_t j = 0; j < t->child_count; j++) {
        Child *c = &t->children[j];
        if (c->is_text) {
            const char *text = c->text;
            size_t text_len  = c->text_len;
            if (text && memchr(text, '\x7F', text_len)) {
                /* This token (t) has a text child with sentinels.
                 * Rebuild t from this text, which replaces all children.
                 * Note: build_from_str frees all children (including this
                 * text child) and replaces them. After this call, child
                 * positions shift, so we exit the loop. */
                build_from_str(t, text, text_len, accum);
                break;
            }
        } else if (c->token) {
            /* Recurse into child token */
            build_token_recursive(c->token, accum);
        }
    }
}

void build(Token *root, const ThreadBuf *tb, Accum *accum)
{
    /* Step 1: Expand the root's working string (which has embedded \0 sentinels). */
    build_from_str(root, tb->buf, tb->len, accum);

    /* Step 2: Recursively build any sub-tokens whose text children contain
     * sentinel markers (e.g. ext-inner content from later stages, or nested
     * templates whose parameter-value contains a sentinel for an inner token). */
    for (size_t i = 0; i < accum->count; i++) {
        Token *t = accum->tokens[i];
        if (!t || t == root) continue;
        build_token_recursive(t, accum);
    }
}
