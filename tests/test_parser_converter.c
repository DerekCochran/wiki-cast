#include <stdio.h>
#include <stdlib.h>
#include "wiki_cast/config.h"
#include "test_common.h"

#ifndef CONFIG_PATH
#define CONFIG_PATH "../config/default.json"
#endif

static const char *samples[] = {
    "-{text}-",
    "-{zh-hans:简体;zh-hant:繁體}-",
    "-{zh:漢字;zh-hans:汉字;zh-hant:漢字}-",
    "-{A|zh-hans:简体;zh-hant:繁體}-",
    "-{R|raw text}-",
    "-{zh-hans:-{inner}-;zh-hant:outer}-",
    "{{Template|-{zh-hans:简;zh-hant:繁}-}}",
    "plain text without converter",
    "''italic'' -{ zh-hans:汉字 }- text",
    "-{}-",
    "-{|zh-hans:简体;zh-hant:繁體}-",
    /* Nested converter edge cases */
    "-{zh-hans:A-{zh-hant:B}-C;zh-hant:D}-",
    "text-{A|zh-hans:-{inner:val}-;zh-hant:outer}-more",
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

    size_t failed = run_parser_samples("converter", samples, sizeof(samples) / sizeof(samples[0]), cfg, false, 10);
    config_free(cfg);
    return failed > 0 ? 1 : 0;
}
