/* test_wikitext.c — Parse every file in tests/wikitext, sorted lexicographically, and print JSON output. */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
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
                                   char ***names_out,
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

    *samples_out = samples;
    *names_out = names;
    *count_out = count;
    return true;
}

static bool load_file_samples(const char *path,
                              char ***samples_out,
                              char ***names_out,
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

    char **names = malloc(sizeof(char *));
    if (!names) {
        fprintf(stderr, "ERROR: malloc\n");
        free(samples[0]);
        free(samples);
        return false;
    }

    names[0] = strdup(path);
    if (!names[0]) {
        fprintf(stderr, "ERROR: malloc\n");
        free(names);
        free(samples[0]);
        free(samples);
        return false;
    }

    *samples_out = samples;
    *names_out = names;
    *count_out = 1;
    return true;
}

static size_t read_rss_kb(void)
{
    FILE *fp = fopen("/proc/self/status", "r");
    if (!fp) {
        return 0;
    }

    char line[256];
    size_t rss_kb = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            unsigned long v = 0;
            if (sscanf(line + 6, "%lu", &v) == 1) {
                rss_kb = (size_t)v;
            }
            break;
        }
    }

    fclose(fp);
    return rss_kb;
}

static void log_thread_buffer_stats(const char *sample_name, size_t sample_index)
{
    ThreadBuffers *tb = wiki_thread_buf_get();
    if (!tb) return;

    size_t scratch_in_use = 0;
    size_t scratch_heap_cap = 0;
    size_t scratch_heap_count = 0;
    for (size_t i = 0; i < tb->scratch_count; ++i) {
        ThreadBuf *s = tb->scratch_pool ? tb->scratch_pool[i] : NULL;
        bool in_use = tb->scratch_in_use ? tb->scratch_in_use[i] : false;
        if (in_use) scratch_in_use++;
        if (s && s->is_on_heap) {
            scratch_heap_count++;
            scratch_heap_cap += s->cap;
        }
    }

    size_t rss_kb = read_rss_kb();
    printf("MEM sample=%zu name=%s rss_kb=%zu stage_cap=%zu stage_len=%zu tokens_cap=%zu tokens_len=%zu scratch_count=%zu scratch_in_use=%zu scratch_heap_count=%zu scratch_heap_cap=%zu\n",
           sample_index + 1,
           sample_name ? sample_name : "(null)",
           rss_kb,
           tb->stage.cap,
           tb->stage.len,
           tb->tokens.cap,
           tb->tokens.len,
           tb->scratch_count,
           scratch_in_use,
           scratch_heap_count,
           scratch_heap_cap);
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
    char **names = NULL;
    size_t sample_count = 0;
    bool loaded = false;
    if (S_ISDIR(st.st_mode)) {
        loaded = load_directory_samples(input_path, &samples, &names, &sample_count);
    } else {
        loaded = load_file_samples(input_path, &samples, &names, &sample_count);
    }

    if (!loaded) {
        config_free(cfg);
        return 1;
    }


    size_t total_failed = 0;
    bool debug_mem = getenv("WTC_DEBUG_RSS_EACH") != NULL;
    for (size_t i = 0; i < sample_count; ++i) {
        const char *single_sample = samples[i];
        // Get the current time for logging of how long it took.
        struct timespec start_time, end_time;
        clock_gettime(CLOCK_MONOTONIC, &start_time);

        size_t failed = run_parser_samples("wikitext", &single_sample, 1, cfg, false, 10);
        // Get elapsed time in ms
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        long elapsed_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 +
                          (end_time.tv_nsec - start_time.tv_nsec) / 1000000;

        if (debug_mem) {
            // Add timing output for how long the single sample took.
            printf("TIME sample=%zu name=%s elapsed_ms=%ld\n",
                   i + 1, names[i], elapsed_ms);
            log_thread_buffer_stats(names[i], i);
        }
        if (failed > 0) {
            printf("FAILED: %s\n", names[i]);
            total_failed++;
        }
    }

    for (size_t i = 0; i < sample_count; ++i) {
        free(samples[i]);
        free(names[i]);
    }
    free(samples);
    free(names);
    config_free(cfg);
    return total_failed == 0 ? 0 : 1;
}
