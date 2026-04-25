#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "test_common.h"

#ifndef CONFIG_PATH
#define CONFIG_PATH "../config/default.json"
#endif

static const char *samples[] = {
    "* Item 1\n* Item 2\n* Item 3",
    "# One\n# Two\n# Three",
    "; Term : Definition",
    ": just a definition",
    "* Bullet\n# Number\n* Bullet again",
    "* Level 1\n** Level 2\n** Level 2 again\n* Level 1 again",
    "# First\n## Sub-first\n## Sub-second\n# Second",
    "* Bullet\n*# Numbered sub-item\n* Bullet again",
    "* ''italic'' item",
    "* [[Main Page|home page]] item",
    ": indented paragraph",
    "**** fourth level",
    "* First list\n\n* Second list",
    "* item 1\nplain text\n* item 2",
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

    size_t failed = run_parser_samples("list", samples, sizeof(samples) / sizeof(samples[0]), cfg, false, 9);
    config_free(cfg);
    return failed > 0 ? 1 : 0;
}
