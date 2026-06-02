#include <stdio.h>
#include <stdlib.h>
#include "wiki_cast/config.h"
#include "test_common.h"

#ifndef CONFIG_PATH
#define CONFIG_PATH "../config/default.json"
#endif

static const char *samples[] = {
    "{|\n|-\n| cell\n|}",
    "{| class=\"wikitable\"\n|+ Caption\n|-\n| A || B\n|-\n| C || D\n|}",
    "{|\n| R1C1 || R1C2 || R1C3\n|-\n| R2C1 || R2C2 || R2C3\n|}",
    "{|\n! Header1 !! Header2\n|-\n| data1 || data2\n|}",
    "{|\n|- class=\"odd\"\n| cell\n|}",
    "{|\n| outer || {|\n| inner\n|}\n|}",
    "prefix\n{|\n| cell\n|}\nsuffix",
    "{|\n| style=\"color:red\" | red text\n|}",
    "{|\n| {{green}} | x\n|}",
    "| {{chset-cell1 | 123 U+007B: LEFT CURLY BRACKET | [[Left curly bracket|{]] | style=background:#ffffb2}}",
    "{|\n| {{chset-cell1 | 123 U+007B: LEFT CURLY BRACKET | [[Left curly bracket|{]] | style=background:#ffffb2}}\n|}",
    "{|\n|}",
    ":{|\n| cell\n|}",
    "{|\n| cell\n|} trailing text",
    "{|\n|+ style=\"font-weight:bold\" | Important\n|-\n| data\n|}",
    "{| class=\"wikitable\"\n|-\n|Μῆνιν ἄειδε θεὰ Πηληιάδεω Ἀχιλῆος<br />\nοὐλομένην, ἣ μυρί' Ἀχαιοῖς ἄλγε' ἔθηκε, [...]\n|Sing, Goddess, of the rage of Peleus' son Achilles,<br />\nthe accursed rage that brought great suffering to the Achaeans, [...]\n|}",
    /* regression: template nested in table caption was dropped */
    "{|class=\"wikitable\" style=\"border: none; float: right;\"\n|+ Anarchist vs. statist perspectives on education<br/>{{Small|Ruth Kinna (2019){{Sfn|Kinna|2019|p=97}}}}\n|-\n!scope=\"col\"|\n!scope=\"col\"|Anarchist education\n!scope=\"col\"|State education\n|}",
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

    size_t failed = run_parser_samples("table", samples, sizeof(samples) / sizeof(samples[0]), cfg, false, 3);
    config_free(cfg);
    return failed > 0 ? 1 : 0;
}
