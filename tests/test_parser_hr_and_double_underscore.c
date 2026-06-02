#include <stdio.h>
#include <stdlib.h>
#include "wiki_cast/config.h"
#include "test_common.h"

#ifndef CONFIG_PATH
#define CONFIG_PATH "../config/default.json"
#endif

static const char *samples[] = {
    "Line above\n----\nLine below",
    "Line above\n-----------\nLine below",
    "Line above\n---\nLine below",
    "__NOTOC__ in text",
    "==Section==\n__TOC__\nContent",
    "Start\n__FORCETOC__\nEnd",
    "__NOEDITSECTION__\n== Section ==",
    "__NEWSECTIONLINK__",
    "__NOTC__",
    "__nOtC__",
    "__NOCC__",
    "__DISAMBIG__",
    "__EXPECTED_UNCONNECTED_PAGE__",
    "__NOTAKEYWORD__",
    "== Section Title ==\nContent here.",
    "=== Level 3 ===",
    "{|\n|-\n| before\n----\nafter\n|}",
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

    size_t failed = run_parser_samples("hr_and_double_underscore", samples, sizeof(samples) / sizeof(samples[0]), cfg, false, 4);
    config_free(cfg);
    return failed > 0 ? 1 : 0;
}
