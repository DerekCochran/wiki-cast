#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "parse.h"
#include "config.h"
#include "accum.h"
#include "string_util.h"
#include "thread_buffer.h"
#include "token.h"
#include "parser/table.h"
#include "log.h"

#ifndef CONFIG_PATH
#define CONFIG_PATH "../config/default.json"
#endif

static char *read_stdin(size_t *out_len)
{
    size_t cap = 4096;
    char *buf = malloc(cap);
    assert(buf);
    size_t len = 0;
    int ch;
    while ((ch = fgetc(stdin)) != EOF) {
        if (len + 1 >= cap) {
            cap *= 2;
            buf = realloc(buf, cap);
            assert(buf);
        }
        buf[len++] = (char)ch;
    }
    buf[len] = '\0';
    if (out_len) *out_len = len;
    return buf;
}

/*
 * Recursively append all text from a token tree to a growable buffer.
 * This mirrors JS token.toString(): concatenate each child's text in order.
 */
static void token_stringify(const Token *t,
                            char **bufp, size_t *lenp, size_t *capp)
{
    if (!t) return;
    for (size_t i = 0; i < t->child_count; i++) {
        const Child *c = &t->children[i];
        if (c->is_text) {
            size_t slen = strlen(c->text);
            while (*lenp + slen + 1 > *capp) {
                *capp *= 2;
                *bufp = realloc(*bufp, *capp);
                assert(*bufp);
            }
            memcpy(*bufp + *lenp, c->text, slen);
            *lenp += slen;
        } else {
            token_stringify(c->token, bufp, lenp, capp);
        }
    }
}

int main(int argc, char **argv)
{
    /* Read input from stdin (raw bytes) */
    size_t in_len = 0;
    char *input = read_stdin(&in_len);
    if (!input) return 1;

    /* Load config file (path from env or default relative to repo) */
    const char *cfg_path = getenv("WIKI_CONFIG");
    if (!cfg_path) cfg_path = CONFIG_PATH;
    ParserConfig *cfg = config_load_file(cfg_path);
    if (!cfg) {
        log_error("ERROR: cannot load config %s", cfg_path);
        free(input);
        return 2;
    }

    /* Prepare working buffer and Accum */
    ThreadBuffers *tbufs = wiki_thread_buf_get();
    ThreadBuf *ws = &tbufs->scratch;
    wiki_thread_buf_set(ws, input, in_len);
    free(input);

    Accum accum;
    accum_init(&accum);

    /* Run stage 3 (parse_table) */
    parse_table(ws, cfg, &accum);

    /*
    /* Resolve sentinels in the thread buffer back to token text, mirroring
     * JS token.toString() after build().  Sentinel format: \0<decimal>X\x7F
     */
    size_t out_cap = ws->len * 4 + 64;
    char *out_text = malloc(out_cap);
    assert(out_text);
    size_t out_len = 0;

    for (size_t i = 0; i < ws->len; ) {
        unsigned char c = (unsigned char)ws->buf[i];
        if (c == '\0') {
            size_t k = i + 1;
            size_t val = 0;
            int any = 0;
            while (k < ws->len && ws->buf[k] >= '0' && ws->buf[k] <= '9') {
                any = 1;
                val = val * 10 + (size_t)(ws->buf[k] - '0');
                k++;
            }
            /* sentinel: \0 <digits> <type-char> \x7F */
            if (any && k < ws->len && k + 1 < ws->len &&
                (unsigned char)ws->buf[k + 1] == '\x7F') {
                Token *tok = accum_get(&accum, val);
                if (tok) {
                    token_stringify(tok, &out_text, &out_len, &out_cap);
                }
                i = k + 2;
                continue;
            }
            /* not a valid sentinel — copy raw NUL byte */
        }
        while (out_len + 1 >= out_cap) {
            out_cap *= 2;
            out_text = realloc(out_text, out_cap);
            assert(out_text);
        }
        out_text[out_len++] = ws->buf[i++];
    }
    out_text[out_len] = '\0';

    /* Write resolved text to stdout (matches JS token.toString()) */
    fwrite(out_text, 1, out_len, stdout);
    free(out_text);

    /* Free tokens in accum shallowly to avoid double-free (children are
     * also present in accum). We free each token's own memory and text
     * children but do not recurse into token children here. */
    for (size_t ti = 0; ti < accum.count; ti++) {
        if (accum.tokens[ti]) token_free_shallow(accum.tokens[ti]);
    }

    accum_free(&accum);
    config_free(cfg);

    return 0;
}
