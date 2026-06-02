#include <stdio.h>
#include <stdlib.h>
#include "wiki_cast/config.h"
#include "test_common.h"

#ifndef CONFIG_PATH
#define CONFIG_PATH "../config/default.json"
#endif

static const char *samples[] = {
    "[http://example.com]",
    "[http://example.com Example site]",
    "[https://secure.example.org/path Secure site]",
    "[ftp://files.example.com FTP link]",
    "[//example.com Protocol-relative]",
    "[http://a.org A] and [http://b.org B]",
    "Visit [http://example.com this site] for more.",
    "[http://example.com?q=foo&bar=baz]",
    "[http://example.com#anchor Anchor link]",
    "[http://example.com/path/(parens)/here Label]",
    "bare http://example.com in text",
    "[http://example.com unclosed",
    /* More external link patterns */
    "[http://example.com/path]",
    "[https://api.example.org/v1/endpoint API endpoint]",
    "[https://example.com\tlabel]",
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

    size_t failed = run_parser_samples("external_links", samples, sizeof(samples) / sizeof(samples[0]), cfg, false, 7);
    config_free(cfg);
    return failed > 0 ? 1 : 0;
}
