#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "test_common.h"

#ifndef CONFIG_PATH
#define CONFIG_PATH "../config/default.json"
#endif

static const char *samples[] = {
    "This is ''italic'' text.",
    "This is '''bold''' text.",
    "This is '''''bold italic''''' text.",
    "''italic'' and '''bold''' on one line.",
    "''unbalanced italic",
    "''''four apostrophes''''",
    "''''''six apostrophes''''''",
    "'''bold ''both''' italic''",
    "'''bold\nitalic''",
    "''a'' '''b''' ''c'''",
    "text '''bold with space before''' text",
    "'' '''",
    "{{Template|''arg''}}",
    "{{Template|1=''arg''}}",
    "{{Template|x=''arg''}}",
    "[[Page|''italic display'']]",
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

    size_t failed = run_parser_samples("quotes", samples, sizeof(samples) / sizeof(samples[0]), cfg, false, 6);
    config_free(cfg);
    return failed > 0 ? 1 : 0;
}
