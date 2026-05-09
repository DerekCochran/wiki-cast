/* test_wikitext.c — Parse every file in tests/wikitext, sorted lexicographically, and print JSON output. */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include "parse.h"
#include "token.h"
#include "config.h"
#include "test_common.h"

#ifndef CONFIG_PATH
#define CONFIG_PATH "../config/enwiki.json"
#endif

static int compare_filenames(const void *a, const void *b)
{
    const char * const *pa = a;
    const char * const *pb = b;
    return strcmp(*pa, *pb);
}

static char *join_path(const char *dir, const char *name)
{
    size_t dir_len = strlen(dir);
    bool has_slash = dir_len > 0 && dir[dir_len - 1] == '/';
    size_t name_len = strlen(name);
    char *path = malloc(dir_len + (has_slash ? 0 : 1) + name_len + 1);
    if (!path) {
        return NULL;
    }

    memcpy(path, dir, dir_len);
    if (!has_slash) {
        path[dir_len] = '/';
        dir_len += 1;
    }
    memcpy(path + dir_len, name, name_len);
    path[dir_len + name_len] = '\0';
    return path;
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "ERROR: Could not open %s\n", path);
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        perror("fseek");
        fclose(f);
        return NULL;
    }

    long len = ftell(f);
    if (len < 0) {
        perror("ftell");
        fclose(f);
        return NULL;
    }
    rewind(f);

    char *buf = malloc((size_t)len + 1);
    if (!buf) {
        fprintf(stderr, "ERROR: malloc\n");
        fclose(f);
        return NULL;
    }

    size_t r = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[r] = '\0';
    return buf;
}

static bool load_directory_samples(const char *dir_path,
                                   char ***samples_out,
                                   size_t *count_out)
{
    DIR *dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "ERROR: Could not open directory %s: %s\n", dir_path, strerror(errno));
        return false;
    }

    char **names = NULL;
    size_t count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        char *path = join_path(dir_path, entry->d_name);
        if (!path) {
            fprintf(stderr, "ERROR: malloc\n");
            closedir(dir);
            for (size_t i = 0; i < count; ++i) {
                free(names[i]);
            }
            free(names);
            return false;
        }

        struct stat st;
        if (stat(path, &st) != 0) {
            free(path);
            continue;
        }
        free(path);

        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        char *name_copy = strdup(entry->d_name);
        if (!name_copy) {
            fprintf(stderr, "ERROR: malloc\n");
            closedir(dir);
            for (size_t i = 0; i < count; ++i) {
                free(names[i]);
            }
            free(names);
            return false;
        }

        char **new_names = realloc(names, sizeof(char *) * (count + 1));
        if (!new_names) {
            fprintf(stderr, "ERROR: realloc\n");
            free(name_copy);
            closedir(dir);
            for (size_t i = 0; i < count; ++i) {
                free(names[i]);
            }
            free(names);
            return false;
        }

        names = new_names;
        names[count++] = name_copy;
    }

    closedir(dir);

    if (count == 0) {
        fprintf(stderr, "ERROR: no files found in directory %s\n", dir_path);
        free(names);
        return false;
    }

    qsort(names, count, sizeof(char *), compare_filenames);

    char **samples = malloc(sizeof(char *) * count);
    if (!samples) {
        fprintf(stderr, "ERROR: malloc\n");
        for (size_t i = 0; i < count; ++i) {
            free(names[i]);
        }
        free(names);
        return false;
    }

    bool ok = true;
    for (size_t i = 0; i < count; ++i) {
        char *path = join_path(dir_path, names[i]);
        if (!path) {
            ok = false;
            break;
        }

        samples[i] = read_file(path);
        free(path);
        if (!samples[i]) {
            ok = false;
            break;
        }
    }

    for (size_t i = 0; i < count; ++i) {
        free(names[i]);
    }
    free(names);

    if (!ok) {
        for (size_t i = 0; i < count; ++i) {
            free(samples[i]);
        }
        free(samples);
        return false;
    }

    *samples_out = samples;
    *count_out = count;
    return true;
}

static bool load_file_samples(const char *path,
                              char ***samples_out,
                              size_t *count_out)
{
    char **samples = malloc(sizeof(char *));
    if (!samples) {
        fprintf(stderr, "ERROR: malloc\n");
        return false;
    }

    samples[0] = read_file(path);
    if (!samples[0]) {
        free(samples);
        return false;
    }

    *samples_out = samples;
    *count_out = 1;
    return true;
}

int main(int argc, char **argv)
{
    /* Order of precedence: argv[1] -> WIKI_CONFIG env -> CONFIG_PATH */
    const char *config_path = NULL;
    if (argc > 1 && argv[1] && argv[1][0] != '\0') config_path = argv[1];
    if (!config_path) config_path = getenv("WIKI_CONFIG");
    if (!config_path) config_path = CONFIG_PATH;

    ParserConfig *cfg = config_load_file(config_path);
    if (!cfg) {
        fprintf(stderr, "ERROR: Could not load config from %s\n", config_path);
        return 1;
    }

    /* Order of precedence for input path: argv[2] -> WIKI_FILE env -> default */
    const char *input_path = NULL;
    if (argc > 2 && argv[2] && argv[2][0] != '\0') input_path = argv[2];
    if (!input_path) input_path = getenv("WIKI_FILE");
    if (!input_path) input_path = "tests/wikitext";

    struct stat st;
    if (stat(input_path, &st) != 0) {
        fprintf(stderr, "ERROR: Could not stat %s\n", input_path);
        config_free(cfg);
        return 1;
    }

    char **samples = NULL;
    size_t sample_count = 0;
    bool loaded = false;
    if (S_ISDIR(st.st_mode)) {
        loaded = load_directory_samples(input_path, &samples, &sample_count);
    } else {
        loaded = load_file_samples(input_path, &samples, &sample_count);
    }

    if (!loaded) {
        config_free(cfg);
        return 1;
    }

    size_t failed = run_parser_samples("wikitext", (const char **)samples, sample_count, cfg, false, 10);
    for (size_t i = 0; i < sample_count; ++i) {
        free(samples[i]);
    }
    free(samples);
    config_free(cfg);
    return failed == 0 ? 0 : 1;
}
