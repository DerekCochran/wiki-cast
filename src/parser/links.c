#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "parser/links.h"
#include "string_util.h"
#include "token.h"
#include "accum.h"
#include "title.h"
#include "build.h"
#include "parser/braces.h"
#include "parser/external_links.h"
#include "parser/quotes.h"
#include "parser/magic_links.h"
#include "parser/comment_and_ext.h"
#include "parser/html.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <strings.h>

/* Forward declaration for helper defined later in this file. */
static Token *parse_inner_fragment(const char *s, size_t len, const ParserConfig *cfg, Accum *accum, const char *type_name, bool tidy, bool in_file);
static void trim_view(const char **ptr, size_t *len);

static int eq_ci_n(const char *a, size_t alen, const char *b)
{
    size_t blen = strlen(b);
    if (alen != blen) return 0;
    return strncasecmp(a, b, alen) == 0;
}

static Token *make_image_param_token(const char *name, Accum *accum)
{
    Token *p = token_new(TOKEN_PLAIN, "image-parameter");
    if (!p) return NULL;
    p->name = strdup(name);
    if (!p->name) {
        token_free(p);
        return NULL;
    }
    accum_push(accum, p);
    return p;
}

static void set_image_param_syntax(Token *param, const char *syntax)
{
    if (!param || !syntax) return;
    param->data.image_param.raw_syntax = strdup(syntax);
}

static void set_image_param_syntax_n(Token *param, const char *syntax, size_t syntax_len)
{
    if (!param || !syntax) return;
    param->data.image_param.raw_syntax = malloc(syntax_len + 1);
    assert(param->data.image_param.raw_syntax);
    memcpy(param->data.image_param.raw_syntax, syntax, syntax_len);
    param->data.image_param.raw_syntax[syntax_len] = '\0';
}

static bool match_img_syntax(const char *seg, size_t seg_len,
                             const char *syntax,
                             const char **cap_ptr, size_t *cap_len,
                             bool *has_cap)
{
    const char *slot = strstr(syntax, "$1");
    if (!slot) {
        if (has_cap) *has_cap = false;
        if (cap_ptr) *cap_ptr = NULL;
        if (cap_len) *cap_len = 0;
        return eq_ci_n(seg, seg_len, syntax) != 0;
    }

    size_t pre_len = (size_t)(slot - syntax);
    size_t suf_len = strlen(slot + 2);
    if (seg_len < pre_len + suf_len) return false;
    if (pre_len > 0 && strncasecmp(seg, syntax, pre_len) != 0) return false;
    if (suf_len > 0 && strncasecmp(seg + seg_len - suf_len, slot + 2, suf_len) != 0) return false;

    if (has_cap) *has_cap = true;
    if (cap_ptr) *cap_ptr = seg + pre_len;
    if (cap_len) *cap_len = seg_len - pre_len - suf_len;
    return true;
}

static bool syntax_ends_with_slot(const char *syntax)
{
    size_t n = strlen(syntax);
    return n >= 2 && syntax[n - 2] == '$' && syntax[n - 1] == '1';
}

static char *build_img_syntax_template(const char *seg_ptr, size_t seg_len,
                                       size_t lead_ws_len, size_t trail_ws_len,
                                       const char *syntax,
                                       bool slot_at_end)
{
    size_t syntax_len = strlen(syntax);
    size_t tmpl_trail_ws_len = slot_at_end ? 0 : trail_ws_len;
    size_t out_len = lead_ws_len + syntax_len + tmpl_trail_ws_len;
    char *out = malloc(out_len + 1);
    if (!out) return NULL;
    if (lead_ws_len > 0) memcpy(out, seg_ptr, lead_ws_len);
    memcpy(out + lead_ws_len, syntax, syntax_len);
    if (tmpl_trail_ws_len > 0) {
        memcpy(out + lead_ws_len + syntax_len,
               seg_ptr + seg_len - tmpl_trail_ws_len,
               tmpl_trail_ws_len);
    }
    out[out_len] = '\0';
    return out;
}

static void append_fragment_children(Token *dst, Token *frag)
{
    if (!dst || !frag) return;
    for (size_t ci = 0; ci < frag->child_count; ci++) {
        if (frag->children[ci].is_text) {
            token_append_text_n(dst, frag->children[ci].text, frag->children[ci].text_len);
        } else {
            token_append_child(dst, frag->children[ci].token);
            frag->children[ci].token = NULL;
        }
    }
}

static void append_file_image_params(Token *file_tok,
                                     const char *text_ptr, size_t text_len,
                                     const ParserConfig *cfg, Accum *accum,
                                     bool tidy)
{
    if (!file_tok || !text_ptr) return;

    size_t seg_start = 0;
    int conv_depth = 0;

    while (seg_start <= text_len) {
        size_t seg_end = seg_start;
        while (seg_end < text_len) {
            if (seg_end + 1 < text_len && text_ptr[seg_end] == '-' && text_ptr[seg_end + 1] == '{') {
                conv_depth++;
                seg_end += 2;
                continue;
            }
            if (seg_end + 1 < text_len && text_ptr[seg_end] == '}' && text_ptr[seg_end + 1] == '-' && conv_depth > 0) {
                conv_depth--;
                seg_end += 2;
                continue;
            }
            if (text_ptr[seg_end] == '|' && conv_depth == 0) {
                break;
            }
            seg_end++;
        }

        const char *seg_ptr = text_ptr + seg_start;
        size_t seg_len = seg_end - seg_start;

        const char *match_ptr = seg_ptr;
        size_t match_len = seg_len;
        trim_view(&match_ptr, &match_len);
        size_t lead_ws_len = (size_t)(match_ptr - seg_ptr);
        size_t trail_ws_len = seg_len - lead_ws_len - match_len;

        /* Note: Do NOT trim whitespace from image parameters - JavaScript preserves it */

        bool is_single_empty = (text_len == 0 && seg_start == 0 && seg_len == 0);
        if (seg_len > 0 || is_single_empty) {
            Token *param = NULL;
            bool matched = false;

            if (cfg && cfg->img.count > 0) {
                for (size_t i = 0; i < cfg->img.count; i++) {
                    const char *syntax = cfg->img.keys[i];
                    const char *name = cfg->img.values[i];
                    const char *cap_ptr = NULL;
                    size_t cap_len = 0;
                    bool has_cap = false;
                    if (!syntax || !name) continue;

                    if (!match_img_syntax(match_ptr, match_len, syntax, &cap_ptr, &cap_len, &has_cap)) continue;

                    param = make_image_param_token(name, accum);
                    if (!param) {
                        matched = true;
                        break;
                    }

                    if (has_cap) {
                        const char *vp = cap_ptr;
                        size_t vl = cap_len;
                        bool slot_at_end = syntax_ends_with_slot(syntax);
                        if (slot_at_end) {
                            vl += trail_ws_len;
                        }
                        /* Note: Do NOT trim the value - JavaScript parser preserves whitespace */

                        Token *val = parse_inner_fragment(vp, vl, cfg, accum, "text", tidy, true);
                        if (val) {
                            append_fragment_children(param, val);
                            token_free(val);
                        }

                        char *syn = build_img_syntax_template(seg_ptr, seg_len,
                                                             lead_ws_len, trail_ws_len,
                                                             syntax, slot_at_end);
                        if (syn) {
                            set_image_param_syntax(param, syn);
                            free(syn);
                        }
                    } else {
                        set_image_param_syntax_n(param, seg_ptr, seg_len);
                    }

                    matched = true;
                    break;
                }
            }

            if (!matched) {
                param = make_image_param_token("caption", accum);
                if (param) {
                    Token *cap = parse_inner_fragment(seg_ptr, seg_len, cfg, accum, "text", tidy, true);
                    if (cap) {
                        append_fragment_children(param, cap);
                        token_free(cap);
                    }
                }
            }

            if (param) {
                token_append_child(file_tok, param);
            }
        }

        if (seg_end >= text_len) break;
        seg_start = seg_end + 1;
    }
}

/* ---- Static compiled regexes (equivalent to JS, compiled once) ----
 *
 * JS: const regexImg = /^((?:(?!\0\d+!\x7F)[^\n[\]{}|])+)(\||\0\d+!\x7F)([\s\S]*)$/u;
 * JS (inExt=false): /^((?:(?!\0\d+!\x7F)[^\n[\]{}|])*)(?:(\||\0\d+!\x7F)([\s\S]*?[^\]])?)?\]\]([\s\S]*)$/u
 * JS: /\0\d+[exhbru]\x7F/u
 */
static pcre2_code *s_re_main     = NULL;  /* inExt=false link regex */
static pcre2_code *s_re_main_ext = NULL;  /* inExt=true link regex */
static pcre2_code *s_re_img      = NULL;  /* regexImg */
static pcre2_code *s_re_sentinel = NULL;  /* \0\d+[exhbru]\x7F */

static void ensure_link_regexes(void)
{
    if (s_re_main && s_re_main_ext && s_re_img && s_re_sentinel) return;

    PCRE2_SIZE err_off; int err_code;

    if (!s_re_main) {
        /* JS: /^((?:(?!\0\d+!\x7F)[^\n[\]{}|])*)(?:(\||\0\d+!\x7F)([\s\S]*?[^\]])?)?\]\]([\s\S]*)$/u */
        const char *pat =
            "^((?:(?!\\x00\\d+!\\x7F)[^\\n[\\]{}|])*)(?:(\\||\\x00\\d+!\\x7F)([\\s\\S]*?[^\\]])?)?\\]\\]([\\s\\S]*)$";
        s_re_main = pcre2_compile((PCRE2_SPTR)pat, PCRE2_ZERO_TERMINATED,
                                   PCRE2_UTF, &err_code, &err_off, NULL);
        if (!s_re_main) {
            PCRE2_UCHAR8 ebuf[256];
            pcre2_get_error_message(err_code, ebuf, sizeof(ebuf));
            log_error("links re_main compile error at %zu: %s", (size_t)err_off, ebuf);
        }
    }

    if (!s_re_main_ext) {
        /* JS inExt=true:
         * /^((?:(?!\0\d+!\x7F)[^\n[\]{}|])+)(?:(\||\0\d+!\x7F)([\s\S]*?[^\]]))?\]\]([\s\S]*)$/u
         */
        const char *pat =
            "^((?:(?!\\x00\\d+!\\x7F)[^\\n[\\]{}|])+)(?:(\\||\\x00\\d+!\\x7F)([\\s\\S]*?[^\\]]))?\\]\\]([\\s\\S]*)$";
        s_re_main_ext = pcre2_compile((PCRE2_SPTR)pat, PCRE2_ZERO_TERMINATED,
                                      PCRE2_UTF, &err_code, &err_off, NULL);
        if (!s_re_main_ext) {
            PCRE2_UCHAR8 ebuf[256];
            pcre2_get_error_message(err_code, ebuf, sizeof(ebuf));
            log_error("links re_main_ext compile error at %zu: %s", (size_t)err_off, ebuf);
        }
    }

    if (!s_re_img) {
        /* JS: /^((?:(?!\0\d+!\x7F)[^\n[\]{}|])+)(\||\0\d+!\x7F)([\s\S]*)$/u */
        const char *pat =
            "^((?:(?!\\x00\\d+!\\x7F)[^\\n[\\]{}|])+)(\\||\\x00\\d+!\\x7F)([\\s\\S]*)$";
        s_re_img = pcre2_compile((PCRE2_SPTR)pat, PCRE2_ZERO_TERMINATED,
                                  PCRE2_UTF, &err_code, &err_off, NULL);
        if (!s_re_img) {
            PCRE2_UCHAR8 ebuf[256];
            pcre2_get_error_message(err_code, ebuf, sizeof(ebuf));
            log_error("links re_img compile error at %zu: %s", (size_t)err_off, ebuf);
        }
    }

    if (!s_re_sentinel) {
        /* JS: /\0\d+[exhbru]\x7F/u */
        const char *pat = "\\x00\\d+[exhbru]\\x7F";
        s_re_sentinel = pcre2_compile((PCRE2_SPTR)pat, PCRE2_ZERO_TERMINATED,
                                       PCRE2_UTF, &err_code, &err_off, NULL);
        if (!s_re_sentinel) {
            PCRE2_UCHAR8 ebuf[256];
            pcre2_get_error_message(err_code, ebuf, sizeof(ebuf));
            log_error("links re_sentinel compile error at %zu: %s", (size_t)err_off, ebuf);
        }
    }
}

/* Compile and cache a protocol-detection regex in cfg->regex_links.
 * JS: config.regexLinks ??= new RegExp(`^\s*(?:${config.protocol}|//)`, 'iu');
 */
static pcre2_code *compile_links_proto(const ParserConfig *cfg)
{
    if (!cfg || !cfg->protocol) return NULL;
    size_t cap = 64 + strlen(cfg->protocol);
    char *pat = malloc(cap);
    assert(pat);
    snprintf(pat, cap, "^\\s*(?:%s|//)", cfg->protocol);

    PCRE2_SIZE err_offset;
    int err_code;
    pcre2_code *re = pcre2_compile((PCRE2_SPTR)pat, PCRE2_ZERO_TERMINATED,
                                    PCRE2_CASELESS | PCRE2_UTF,
                                    &err_code, &err_offset, NULL);
    if (!re) {
        PCRE2_UCHAR8 err_buf[256];
        pcre2_get_error_message(err_code, err_buf, sizeof(err_buf));
        log_error("links proto regex compile error at %zu: %s Pattern: %.200s",
                  err_offset, err_buf, pat);
    }
    free(pat);
    return re;
}

/* Trim whitespace (in-place view) */
static void trim_view(const char **ptr, size_t *len)
{
    const char *s = *ptr;
    size_t l = *len;
    size_t a = 0;
    while (a < l && isspace((unsigned char)s[a])) a++;
    size_t b = l;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    *ptr = s + a;
    *len = b - a;
}

/* Run a PCRE2 regex match on a (possibly NUL-containing) region.
 * Returns the pcre2_match_data on success (caller must free), or NULL. */
static pcre2_match_data *match_region(pcre2_code *re,
                                      const char *s, size_t len)
{
    if (!re || !s) return NULL;
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
    if (!md) return NULL;
    int rc = pcre2_match(re, (PCRE2_SPTR)s, (PCRE2_SIZE)len, 0, 0, md, NULL);
    if (rc < 0) {
        pcre2_match_data_free(md);
        return NULL;
    }
    return md;
}

/* ---- Bit (segment between "[[" markers) ---- */
typedef struct { const char *ptr; size_t len; } Bit;

/* Build the output sentinel marker string into buf (at least 32 bytes).
 * Returns the length written. */
static size_t make_sentinel(size_t idx, char type, char *buf)
{
    size_t n = 0;
    buf[n++] = '\0';
    char tmp[24]; int tl = snprintf(tmp, sizeof(tmp), "%zu", idx);
    memcpy(buf + n, tmp, tl); n += tl;
    buf[n++] = type;
    buf[n++] = '\x7F';
    return n;
}

#define ENSURE_OUT(need) do { \
    while (out_len + (size_t)(need) >= out_cap) { \
        out_cap *= 2; out = realloc(out, out_cap); assert(out); \
    } \
} while(0)

void parse_links(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum,
                 const char *page, bool tidy)
{
    (void)page;
    if (!tb || !tb->buf) return;

    ensure_link_regexes();

    /* Ensure proto regex compiled */
    if (!cfg->regex_links) {
        ParserConfig *m = (ParserConfig *)cfg;
        m->regex_links = (ParserConfigRegex *)compile_links_proto(cfg);
    }
    pcre2_code *re_proto    = (pcre2_code *)cfg->regex_links;
    pcre2_code *re_main     = cfg->in_ext ? s_re_main_ext : s_re_main;
    pcre2_code *re_img_re   = s_re_img;
    pcre2_code *re_sentinel = s_re_sentinel;

    size_t len = tb->len;
    const char *buf = tb->buf;

    /* ---- Split buf on "[[" to get bits ----
     * bits[0]       = everything before first "[[" (initial s = bits.shift())
     * bits[1..n]    = text after each "[[" up to the next "[[" (or end of buf)
     */
    size_t *bb_pos   = NULL;
    size_t  bb_count = 0, bb_cap = 0;
    for (size_t i = 0; i + 1 < len; i++) {
        if ((unsigned char)buf[i] == '[' && (unsigned char)buf[i + 1] == '[') {
            if (bb_count >= bb_cap) {
                bb_cap = bb_cap ? bb_cap * 2 : 16;
                bb_pos = realloc(bb_pos, bb_cap * sizeof(size_t));
                assert(bb_pos);
            }
            bb_pos[bb_count++] = i;
            i++; /* skip second '[' */
        }
    }

    size_t bits_count = bb_count + 1;
    Bit *bits = malloc(bits_count * sizeof(Bit));
    assert(bits);
    bits[0].ptr = buf;
    bits[0].len = (bb_count > 0) ? bb_pos[0] : len;
    for (size_t k = 0; k < bb_count; k++) {
        bits[k + 1].ptr = buf + bb_pos[k] + 2;
        size_t end = (k + 1 < bb_count) ? bb_pos[k + 1] : len;
        bits[k + 1].len = end - (bb_pos[k] + 2);
    }
    free(bb_pos); bb_pos = NULL;

    /* Output buffer */
    size_t out_cap = len * 2 + 64;
    char *out = malloc(out_cap);
    assert(out);
    size_t out_len = 0;

    /* s = bits.shift() */
    ENSURE_OUT(bits[0].len + 1);
    memcpy(out + out_len, bits[0].ptr, bits[0].len);
    out_len += bits[0].len;

    /* Main loop: for (let i = 0; i < bits.length; i++) — bi indexes bits[1..bb_count] */
    for (size_t bi = 1; bi <= bb_count; bi++) {
        const char *x    = bits[bi].ptr;
        size_t      xlen = bits[bi].len;

        bool        mightBeImg = false;
        const char *link_ptr  = NULL; size_t link_len  = 0;
        const char *delim_ptr = NULL; size_t delim_len = 0;
        const char *text_ptr  = NULL; size_t text_len  = 0;
        const char *after_ptr = NULL; size_t after_len = 0;
        bool link_found = false;

        /* Apply main regex (inExt=false) */
        {
            pcre2_match_data *md = match_region(re_main, x, xlen);
            if (md) {
                PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
                link_ptr = x + ov[2]; link_len = ov[3] - ov[2];
                if (ov[4] != PCRE2_UNSET) { delim_ptr = x + ov[4]; delim_len = ov[5] - ov[4]; }
                if (ov[6] != PCRE2_UNSET) { text_ptr  = x + ov[6]; text_len  = ov[7] - ov[6]; }
                if (ov[8] != PCRE2_UNSET) { after_ptr = x + ov[8]; after_len = ov[9] - ov[8]; }
                else                      { after_ptr = x + xlen;  after_len = 0; }
                link_found = true;
                pcre2_match_data_free(md);

                /* JS: if (after.startsWith(']') && text?.includes('[')) { text += ']'; after = after.slice(1); } */
                if (after_len > 0 && after_ptr[0] == ']' && text_ptr) {
                    bool has_bracket = false;
                    for (size_t ti = 0; ti < text_len; ti++) {
                        if (text_ptr[ti] == '[') { has_bracket = true; break; }
                    }
                    if (has_bracket) { text_len += 1; after_ptr += 1; after_len -= 1; }
                }
            }
        }

        /* Fallback: regexImg */
        if (!link_found) {
            pcre2_match_data *md = match_region(re_img_re, x, xlen);
            if (md) {
                PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
                link_ptr = x + ov[2]; link_len = ov[3] - ov[2];
                if (ov[4] != PCRE2_UNSET) { delim_ptr = x + ov[4]; delim_len = ov[5] - ov[4]; }
                if (ov[6] != PCRE2_UNSET) { text_ptr  = x + ov[6]; text_len  = ov[7] - ov[6]; }
                mightBeImg = true;
                link_found = true;
                pcre2_match_data_free(md);
            }
        }

        /* JS: if (link === undefined || regexLinks.test(link) || /\0\d+[exhbru]\x7F/.test(link)) */
        if (!link_found) {
            ENSURE_OUT(2 + xlen + 1);
            out[out_len++] = '['; out[out_len++] = '[';
            memcpy(out + out_len, x, xlen); out_len += xlen;
            continue;
        }

        bool is_proto = false;
        if (re_proto) {
            pcre2_match_data *md = match_region(re_proto, link_ptr, link_len);
            if (md) { is_proto = true; pcre2_match_data_free(md); }
        }
        bool has_sentinel_in_link = false;
        if (!is_proto && re_sentinel) {
            pcre2_match_data *md = match_region(re_sentinel, link_ptr, link_len);
            if (md) { has_sentinel_in_link = true; pcre2_match_data_free(md); }
        }
        if (is_proto || has_sentinel_in_link) {
            ENSURE_OUT(2 + xlen + 1);
            out[out_len++] = '['; out[out_len++] = '[';
            memcpy(out + out_len, x, xlen); out_len += xlen;
            continue;
        }

        /* JS: let trimmed = removeComment(link).trim(); */
        size_t tmp_len = 0;
        char *no_comment = str_remove_comment(link_ptr, link_len, &tmp_len);
        if (!no_comment) {
            ENSURE_OUT(2 + xlen + 1);
            out[out_len++] = '['; out[out_len++] = '[';
            memcpy(out + out_len, x, xlen); out_len += xlen;
            continue;
        }
        const char *trim_ptr = no_comment; size_t trim_len = tmp_len;
        trim_view(&trim_ptr, &trim_len);

        bool force = (trim_len > 0 && trim_ptr[0] == ':');

        /* JS: if (force && mightBeImg) { s += "[[" + x; continue; } */
        if (force && mightBeImg) {
            free(no_comment);
            ENSURE_OUT(2 + xlen + 1);
            out[out_len++] = '['; out[out_len++] = '[';
            memcpy(out + out_len, x, xlen); out_len += xlen;
            continue;
        }

        /* Interwiki detection */
        int  ns        = 0;
        bool interwiki = false;
        size_t iw_consumed = 0;
        char  *iw_pref = str_extract_interwiki(trim_ptr, trim_len, cfg, &iw_consumed);
        if (iw_pref) {
            interwiki = true;
            if (iw_consumed <= trim_len) {
                trim_ptr += iw_consumed; trim_len -= iw_consumed;
            } else {
                free(iw_pref); free(no_comment);
                ENSURE_OUT(2 + xlen + 1);
                out[out_len++] = '['; out[out_len++] = '[';
                memcpy(out + out_len, x, xlen); out_len += xlen;
                continue;
            }
        }

        /* Title pointer (skip leading ':' if force) */
        const char *title_ptr = trim_ptr; size_t title_len = trim_len;
        if (force && title_len > 0) {
            title_ptr++; title_len--;
            while (title_len > 0 && isspace((unsigned char)title_ptr[0])) { title_ptr++; title_len--; }
        }

        bool anchor_only = (title_len > 0 && title_ptr[0] == '#');
        if (!anchor_only && !title_is_valid_half_parsed(title_ptr, title_len, cfg)) {
            free(iw_pref); free(no_comment);
            ENSURE_OUT(2 + xlen + 1);
            out[out_len++] = '['; out[out_len++] = '[';
            memcpy(out + out_len, x, xlen); out_len += xlen;
            continue;
        }

        /* Namespace from title prefix before ':' */
        for (size_t i = 0; i < title_len; i++) {
            if (title_ptr[i] == ':') {
                size_t pre_len = i;
                for (size_t k = 0; k < cfg->ns_count; k++) {
                    const char *nm = cfg->namespaces[k].name;
                    if (nm && strlen(nm) == pre_len
                            && strncasecmp(nm, title_ptr, pre_len) == 0) {
                        ns = cfg->namespaces[k].num;
                        break;
                    }
                }
                break;
            }
        }

        /* ---- mightBeImg: File namespace handling ----
         * JS: else if (mightBeImg) {
         *       if (ns !== 6 || interwiki) { s += "[[" + x; continue; }
         *       let found = false;
         *       for (i++; i < bits.length; i++) {
         *         const next = bits[i], p = next.split(']]');
         *         if (p.length > 2) { found=true; text += "[["+p[0]+"]]"+p[1]; after = p.slice(2).join(']]'); break; }
         *         else if (p.length === 2) { text += "[["+p[0]+"]]"+p[1]; }
         *         else { text += "[["+next; break; }
         *       }
         *       text = parseLinks(text, config, accum, page, tidy);
         *       if (!found) { s += "[[" + link + delimiter + text; continue; }
         *     }
         */
        if (mightBeImg) {
            if (ns != 6 || interwiki) {
                free(iw_pref); free(no_comment);
                ENSURE_OUT(2 + xlen + 1);
                out[out_len++] = '['; out[out_len++] = '[';
                memcpy(out + out_len, x, xlen); out_len += xlen;
                continue;
            }

            size_t img_cap = (text_len ? text_len : 0) + 256;
            char  *img_buf = malloc(img_cap);
            assert(img_buf);
            size_t img_len = 0;
            if (text_ptr && text_len > 0) {
                memcpy(img_buf, text_ptr, text_len);
                img_len = text_len;
            }

#define IMG_APPEND(p, n) do { \
    size_t _n = (n); \
    while (img_len + _n >= img_cap) { img_cap *= 2; img_buf = realloc(img_buf, img_cap); assert(img_buf); } \
    memcpy(img_buf + img_len, (p), _n); img_len += _n; \
} while(0)

            bool        found_close = false;
            const char *img_after   = NULL;
            size_t      img_after_l = 0;

            size_t j;
            for (j = bi + 1; j <= bb_count; j++) {
                const char *next_x    = bits[j].ptr;
                size_t      next_xlen = bits[j].len;

                /* Count ]] in next_x; find positions of first two */
                size_t dd_pos0 = SIZE_MAX, dd_pos1 = SIZE_MAX;
                size_t dd_cnt  = 0;
                for (size_t k = 0; k + 1 < next_xlen; k++) {
                    if (next_x[k] == ']' && next_x[k + 1] == ']') {
                        if (dd_cnt == 0) dd_pos0 = k;
                        else if (dd_cnt == 1) dd_pos1 = k;
                        dd_cnt++;
                        k++;
                    }
                }

                if (dd_cnt == 0) {
                    /* p.length === 1: text += "[[" + next; break */
                    IMG_APPEND("[[", 2);
                    IMG_APPEND(next_x, next_xlen);
                    break;
                } else if (dd_cnt == 1) {
                    /* p.length === 2: text += "[[" + p[0] + "]]" + p[1]; (continue) */
                    IMG_APPEND("[[", 2);
                    IMG_APPEND(next_x, dd_pos0);
                    IMG_APPEND("]]", 2);
                    IMG_APPEND(next_x + dd_pos0 + 2, next_xlen - dd_pos0 - 2);
                } else {
                    /* p.length > 2: found=true
                     * text += "[[" + p[0] + "]]" + p[1]
                     * after = p.slice(2).join(']]')  == next_x[dd_pos1+2 .. end)
                     */
                    found_close = true;
                    IMG_APPEND("[[", 2);
                    IMG_APPEND(next_x, dd_pos0);
                    IMG_APPEND("]]", 2);
                    IMG_APPEND(next_x + dd_pos0 + 2, dd_pos1 - dd_pos0 - 2);
                    img_after   = next_x + dd_pos1 + 2;
                    img_after_l = next_xlen - dd_pos1 - 2;
                    bi = j; /* advance outer loop past all consumed bits */
                    break;
                }
            }

            /* text = parseLinks(text, ...) — recursive call on accumulated image text */
            {
                ThreadBuf tmp_tb;
                tmp_tb.buf = img_buf;
                tmp_tb.len = img_len;
                tmp_tb.cap = img_cap;
                /*
                 * This temporary buffer is not managed by thread_buffer.
                 * Disable shrink-path logic by setting a very large threshold
                 * so parse_links() can safely use reserve/set on this buffer.
                 */
                tmp_tb.shrink_size = (size_t)-1;
                tmp_tb.target_size = img_cap ? img_cap : 64;
                parse_links(&tmp_tb, cfg, accum, page, tidy);
                img_buf = tmp_tb.buf;
                img_len = tmp_tb.len;
                img_cap = tmp_tb.cap;
            }

            /* JS: if (!found) { s += "[[" + link + delimiter + text; continue; } */
            if (!found_close) {
                ENSURE_OUT(2 + link_len + delim_len + img_len + 1);
                out[out_len++] = '['; out[out_len++] = '[';
                if (link_ptr  && link_len  > 0) { memcpy(out + out_len, link_ptr,  link_len);  out_len += link_len; }
                if (delim_ptr && delim_len > 0) { memcpy(out + out_len, delim_ptr, delim_len); out_len += delim_len; }
                if (img_buf   && img_len   > 0) { memcpy(out + out_len, img_buf,   img_len);   out_len += img_len; }
                free(img_buf); free(iw_pref); free(no_comment);
                continue;
            }

            /* Build FILE token */
            {
                const char *tok_text_ptr = img_buf;
                size_t      tok_text_len = img_len;

                size_t tok_idx  = accum->count;
                char   sent[64]; size_t sent_len = make_sentinel(tok_idx, 'l', sent);
                ENSURE_OUT(sent_len + img_after_l + 1);
                memcpy(out + out_len, sent, sent_len); out_len += sent_len;
                if (img_after && img_after_l > 0) {
                    memcpy(out + out_len, img_after, img_after_l); out_len += img_after_l;
                }

                Token *tok = token_new(TOKEN_FILE, "file");
                if (!tok) { free(img_buf); free(iw_pref); free(no_comment); continue; }
                accum_push(accum, tok);

                Token *atom = token_new(TOKEN_ATOM, "link-target");
                if (atom) { token_append_text_n(atom, link_ptr, link_len); token_append_child(tok, atom); }

                if (tok_text_ptr) {
                    append_file_image_params(tok, tok_text_ptr, tok_text_len, cfg, accum, tidy);
                }

                size_t base_len = title_len;
                for (size_t ni = 0; ni < title_len; ni++) {
                    if (title_ptr[ni] == '#') { base_len = ni; break; }
                }
                char *norm = title_normalize(title_ptr, base_len);
                if (norm) tok->name = norm;

                free(img_buf);
            }
            free(iw_pref); free(no_comment);
            continue;
        } /* end mightBeImg */

        /* ---- Normal link token ---- */

        TokenType   ttype = TOKEN_LINK;
        const char *tname = "link";
        if (!force) {
            if      (ns == 6  && !interwiki) { ttype = TOKEN_FILE;     tname = "file"; }
            else if (ns == 14 && !interwiki) { ttype = TOKEN_CATEGORY; tname = "category"; }
        }

        size_t tok_idx = accum->count;
        char   sent[64]; size_t sent_len = make_sentinel(tok_idx, 'l', sent);
        ENSURE_OUT(sent_len + after_len + 1);
        memcpy(out + out_len, sent, sent_len); out_len += sent_len;
        if (after_ptr && after_len > 0) { memcpy(out + out_len, after_ptr, after_len); out_len += after_len; }

        Token *tok = token_new(ttype, tname);
        if (!tok) { free(iw_pref); free(no_comment); continue; }
        accum_push(accum, tok);

        /* link-target atom */
        Token *atom = token_new(TOKEN_ATOM, "link-target");
        if (atom) {
            /* JS constructor receives the raw regex-captured `link` text. */
            token_append_text_n(atom, link_ptr, link_len);
            token_append_child(tok, atom);
        }

        /* JS: if (text === undefined && delimiter) { text = ''; }
         * When delimiter present but text absent, JS creates empty text child. */
        if (text_ptr || delim_ptr) {
            const char *tp = text_ptr ? text_ptr : "";
            size_t      tl = text_ptr ? text_len  : 0;
            bool in_file = (ttype == TOKEN_FILE && !interwiki && !force);
            if (in_file) {
                append_file_image_params(tok, tp, tl, cfg, accum, tidy);
            } else {
                Token *lt = parse_inner_fragment(tp, tl, cfg, accum, "link-text", tidy, in_file);
                if (lt) token_append_child(tok, lt);
            }
        }

        /* Normalized name */
        char *norm = NULL;
        if (interwiki && iw_pref) {
            size_t base_len = title_len;
            for (size_t ni = 0; ni < title_len; ni++) {
                if (title_ptr[ni] == '#') { base_len = ni; break; }
            }
            const char *base_ptr = title_ptr;
            size_t base_trim_len = base_len;
            trim_view(&base_ptr, &base_trim_len);

            size_t pfx_len  = strlen(iw_pref);
            size_t full_len = pfx_len + 1 + base_trim_len;
            char *full = malloc(full_len + 1);
            if (full) {
                memcpy(full, iw_pref, pfx_len);
                full[pfx_len] = ':';
                memcpy(full + pfx_len + 1, base_ptr, base_trim_len);
                full[full_len] = '\0';
                norm = full;
            }
        } else {
            size_t base_len = title_len;
            for (size_t ni = 0; ni < title_len; ni++) {
                if (title_ptr[ni] == '#') { base_len = ni; break; }
            }
            const char *base_ptr = title_ptr;
            size_t base_trim_len = base_len;
            trim_view(&base_ptr, &base_trim_len);

            const char *colon = memchr(base_ptr, ':', base_trim_len);
            if (!interwiki && ns == 0 && colon && (size_t)(colon - base_ptr) == 4
                && strncasecmp(base_ptr, "wikt", 4) == 0) {
                size_t suffix_len = base_trim_len - 5;
                char *tmp = malloc(5 + suffix_len + 1);
                if (tmp) {
                    memcpy(tmp, "Wikt:", 5);
                    if (suffix_len > 0) memcpy(tmp + 5, colon + 1, suffix_len);
                    tmp[5 + suffix_len] = '\0';
                    norm = tmp;
                }
            } else {
                norm = title_normalize(base_ptr, base_trim_len);
            }
        }
        if (norm) tok->name = norm;

        free(iw_pref);
        free(no_comment);
    } /* end for bi */

    out[out_len] = '\0';
    wiki_thread_buf_set(tb, out, out_len);
    free(out);
    free(bits);
}

/* Static helper: parse a fragment into a TOKEN_PLAIN with given type_name. */
static Token *parse_inner_fragment(const char *s, size_t len, const ParserConfig *cfg, Accum *accum, const char *type_name, bool tidy, bool in_file)
{
    if (!s) return NULL;
    ThreadBuf *inner_tb = wiki_thread_buf_acquire_scratch();
    wiki_thread_buf_set(inner_tb, s, len);

    if (in_file) {
        parse_comment_and_ext(inner_tb, cfg, accum, false);
    }
    parse_braces(inner_tb, cfg, accum);
    if (in_file) {
        /* JS parity: file/gallery parameter text supports internal links. */
        parse_html(inner_tb, cfg, accum);
        parse_links(inner_tb, cfg, accum, NULL, tidy);
    }
    parse_quotes(inner_tb, cfg, accum, tidy);
    if (in_file) {
        parse_external_links(inner_tb, cfg, accum, true);
        parse_magic_links(inner_tb, cfg, accum);
    }

    Token *inner = token_new(TOKEN_PLAIN, type_name);
    if (!inner) {
        wiki_thread_buf_release_scratch(inner_tb);
        return NULL;
    }

    build_from_str(inner, inner_tb->buf, inner_tb->len, accum);
    wiki_thread_buf_release_scratch(inner_tb);

    return inner;
}
