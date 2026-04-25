#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "test_common.h"

#ifndef CONFIG_PATH
#define CONFIG_PATH "../config/default.json"
#endif

static const char *samples[] = {
    "An autolink: http://example.com in text.",
    "Secure: https://secure.example.org/path here.",
    "See http://example.com/path?q=1&r=2 for details.",
    "Visit http://example.com. End.",
    "See http://example.com/foo(bar) here.",
    "Download at ftp://files.example.com/file.tar.gz end.",
    "See RFC 2119 for definitions.",
    "see rfc 2119 here.",
    "Reference PMID 12345678 here.",
    "Book ISBN 0-306-40615-2 here.",
    "Book ISBN 978-3-16-148410-0 here.",
    "(ISBN 0-306-40615-2)",
    "wordhttps://example.com not linked.",
    "http://example.com/a&lt;b here.",
};

int main(void)
{
    const char *config_path = getenv("WIKI_CONFIG");
    if (!config_path) config_path = CONFIG_PATH;
    ParserConfig *cfg = config_load_file(config_path);
    if (!cfg) {
        fprintf(stderr, "ERROR: Could not load config from %s\n", config_path);
        return 1;
    }

    size_t failed = run_parser_samples("magic_links", samples, sizeof(samples) / sizeof(samples[0]), cfg, false, 8);
    config_free(cfg);
    return failed > 0 ? 1 : 0;
}
