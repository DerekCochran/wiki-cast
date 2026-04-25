#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "test_common.h"

#ifndef CONFIG_PATH
#define CONFIG_PATH "../config/default.json"
#endif

static const char *samples[] = {
    "#REDIRECT [[Target Page]]",
    "#redirect [[Main Page]]",
    "#Redirect [[Article]]",
    "#REDIRECT: [[Target Page]]",
    "   #REDIRECT [[Target]]",
    "#REDIRECT [[Target]]   ",
    "#REDIRECT [[Target]]\nSome trailing text",
    "#重定向 [[中文页面]]",
    "This is not a redirect",
    "#REDIRECT [[Target\nBroken]]",
    "#REDIRECT [[]]",
    "#REDIRECT [[Target|ignored display]]",
    "#REDIRECT [[Help:Contents]]",
    "#REDIRECT [[Page#Section]]",
    "#REDIRECT[[Target]]",
    "#REDIRECT [[Target<!--c-->]]",
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

    size_t failed = run_parser_samples("redirect", samples, sizeof(samples) / sizeof(samples[0]), cfg, false, 0);
    config_free(cfg);
    return failed > 0 ? 1 : 0;
}
