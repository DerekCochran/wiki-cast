#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include "parse.h"
#include "token.h"
#include "util/thread_buffer.h"

/* Print a unified diff between expected and got using the system diff command. */
static bool ensure_dir(const char *path)
{
    if (mkdir(path, 0777) == 0) {
        return true;
    }
    return errno == EEXIST;
}

static void sanitize_name(const char *src, char *dst, size_t dst_cap)
{
    size_t j = 0;
    if (dst_cap == 0) {
        return;
    }

    for (size_t i = 0; src[i] != '\0' && j + 1 < dst_cap; i++) {
        unsigned char ch = (unsigned char)src[i];
        if ((ch >= 'a' && ch <= 'z') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '_' || ch == '-' || ch == '.') {
            dst[j++] = (char)ch;
        } else {
            dst[j++] = '_';
        }
    }

    if (j == 0) {
        dst[j++] = 's';
        if (j < dst_cap) dst[j++] = 'a';
        if (j < dst_cap) dst[j++] = 'm';
        if (j < dst_cap) dst[j++] = 'p';
        if (j < dst_cap) dst[j++] = 'l';
        if (j < dst_cap) dst[j++] = 'e';
    }
    dst[j] = '\0';
}

static void get_artifact_root_dir(char *out, size_t out_cap)
{
    static bool initialized = false;
    static char cached[256];

    if (!initialized) {
        time_t now = time(NULL);
        struct tm tmv;
        if (localtime_r(&now, &tmv) != NULL) {
            strftime(cached, sizeof(cached), "/tmp/wiki_c_%Y-%m-%dT%H-%M-%S", &tmv);
        } else {
            snprintf(cached, sizeof(cached), "/tmp/wiki_c");
        }
        initialized = true;
    }

    snprintf(out, out_cap, "%s", cached);
}

/* Write a token's JSON tree to a file using the library's json_stringify_wikiparser_node(). */
static void write_pretty_json(FILE *fp, const char *json)
{
    if (!fp || !json) return;

    int indent = 0;
    bool in_string = false;
    bool escape = false;

    for (const char *p = json; *p; p++) {
        char ch = *p;

        if (in_string) {
            fputc(ch, fp);
            if (escape) {
                escape = false;
            } else if (ch == '\\') {
                escape = true;
            } else if (ch == '"') {
                in_string = false;
            }
            continue;
        }

        if (ch == '"') {
            in_string = true;
            fputc(ch, fp);
            continue;
        }

        if (ch == '{' || ch == '[') {
            fputc(ch, fp);
            fputc('\n', fp);
            indent++;
            for (int i = 0; i < indent; i++) fputs("  ", fp);
        } else if (ch == '}' || ch == ']') {
            fputc('\n', fp);
            if (indent > 0) indent--;
            for (int i = 0; i < indent; i++) fputs("  ", fp);
            fputc(ch, fp);
        } else if (ch == ',') {
            fputc(ch, fp);
            fputc('\n', fp);
            for (int i = 0; i < indent; i++) fputs("  ", fp);
        } else if (ch == ':') {
            fputs(": ", fp);
        } else if (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') {
            /* Skip insignificant whitespace outside strings. */
        } else {
            fputc(ch, fp);
        }
    }
    fputc('\n', fp);
}

static void write_token_json(const Token *t, const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) return;
    if (t) {
        ThreadBuf *tb = wiki_thread_buf_acquire_scratch();
        json_stringify_wikiparser_node(t, tb);
        write_pretty_json(fp, tb->buf);
        fputc('\n', fp);
        wiki_thread_buf_release_scratch(tb);
    } else {
        fputs("null\n", fp);
    }
    fclose(fp);
}

/* Print a unified diff between expected and got using deterministic artifact names.
 * Also writes JSON tree files and a tree diff when a parsed Token is supplied. */
static void print_diff(const char *expected,
                       const char *got,
                       const char *parser_name,
                       size_t sample_index)
{
    char safe_name[128];
    char root_dir[256];
    char sub_dir[384];
    char path_exp[512];
    char path_got[512];
    char path_diff[512];

    sanitize_name(parser_name, safe_name, sizeof(safe_name));
    get_artifact_root_dir(root_dir, sizeof(root_dir));
    snprintf(sub_dir, sizeof(sub_dir), "%s/%s", root_dir, safe_name);

    if (!ensure_dir(root_dir) || !ensure_dir(sub_dir)) {
        return;
    }

    snprintf(path_exp, sizeof(path_exp), "%s/expected.%04zu.txt", sub_dir, sample_index);
    snprintf(path_got, sizeof(path_got), "%s/got.%04zu.txt", sub_dir, sample_index);
    snprintf(path_diff, sizeof(path_diff), "%s/diff.%04zu.txt", sub_dir, sample_index);

    FILE *fp = fopen(path_exp, "wb");
    if (fp) {
        fwrite(expected, 1, strlen(expected), fp);
        fclose(fp);
    }

    fp = fopen(path_got, "wb");
    if (fp) {
        fwrite(got, 1, strlen(got), fp);
        fclose(fp);
    }

    /* Build: diff --unified=3 --label expected --label got <exp> <got> > <diff> */
    size_t cmd_len = 256 + strlen(path_exp) + strlen(path_got) + strlen(path_diff);
    char *cmd = malloc(cmd_len);
    snprintf(cmd, cmd_len,
             "diff --unified=3 --label expected --label got %s %s > %s",
             path_exp, path_got, path_diff);
    int status = system(cmd);
    if (status == -1) {
        fprintf(stderr, "Warning: Failed to execute diff command subsystem.\n");
    }    
    free(cmd);

    printf("  expected file: %s\n", path_exp);
    printf("  got file:      %s\n", path_got);
    printf("  diff file:     %s\n", path_diff);
    /* Caller must unlink these when done inspecting. */
}

/* Extended variant: also writes JSON tree of the parsed token and a tree diff. */
static void print_diff_with_tree(const char *expected,
                                 const char *got,
                                 const Token *got_token,
                                 const char *parser_name,
                                 size_t sample_index)
{
    print_diff(expected, got, parser_name, sample_index);

    char safe_name[128];
    char root_dir[256];
    char sub_dir[384];
    char path_json[512];

    sanitize_name(parser_name, safe_name, sizeof(safe_name));
    get_artifact_root_dir(root_dir, sizeof(root_dir));
    snprintf(sub_dir, sizeof(sub_dir), "%s/%s", root_dir, safe_name);

    snprintf(path_json, sizeof(path_json), "%s/tree.%04zu.json", sub_dir, sample_index);
    write_token_json(got_token, path_json);

    printf("  json tree:     %s\n", path_json);
}

static size_t run_parser_samples(const char *parser_name,
                                 const char **samples,
                                 size_t sample_count,
                                 const ParserConfig *cfg,
                                 bool include,
                                 int max_stage)
{
    size_t passed = 0;
    size_t failed = 0;
    for (size_t i = 0; i < sample_count; i++) {
        const char *wikitext = samples[i];
        Token *root = wiki_parse(wikitext, strlen(wikitext), cfg, include, max_stage);
        if (!root) {
            printf("FAIL [%s][%zu] parse returned NULL: %s\n",
                   parser_name, i + 1, wikitext);
            failed++;
            continue;
        }

        ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();
        char *roundtrip = token_to_string(root, scratch);
        if (strcmp(roundtrip, wikitext) != 0) {
            printf("FAIL [%s][%zu] round-trip toString mismatch\n", parser_name, i + 1);
            print_diff_with_tree(wikitext, roundtrip, root, parser_name, i + 1);
            failed++;
            wiki_thread_buf_release_scratch(scratch);
            token_free(root);
            continue;
        }

        wiki_thread_buf_release_scratch(scratch);
        passed++;
        token_free(root);
    }

    printf("SUMMARY [%s] passed=%zu failed=%zu total=%zu\n",
           parser_name, passed, failed, sample_count);

    return failed;
}

#endif /* TEST_COMMON_H */
