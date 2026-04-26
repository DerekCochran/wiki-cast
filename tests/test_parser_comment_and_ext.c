#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "test_common.h"

#ifndef CONFIG_PATH
#define CONFIG_PATH "../config/default.json"
#endif

static const char *samples[] = {
    "before <!--comment--> after",
    "a <!--\n  multi-line\n  comment\n--> b",
    "start <!-- unclosed",
    "<!-- outer <!-- inner --> still outer -->",
    "literal <nowiki>[[notalink]]</nowiki> tail",
    "<nowiki/>",
    "text<ref>Citation here.</ref> end",
    "text<ref name=\"foo\">Named ref.</ref> end",
    "<ref a=b c=\"d\" e>z</ref>",
    "text<ref>Merriman, John M. (2009). ''How a Bombing in Fin-de-Siecle Paris Ignited the Age of Modern Terror''. New Haven: Yale University Press. p. 42. {{ISBN|9780300158864}}</ref> end",
    "text<ref>{{Cite web |title=Foo {{!}} Bar |url=https://example.com}}</ref> end",
    "text<ref name=\"bar\"/> end",
    "<references/>",
    "<gallery>\nFile:Foo.jpg|caption\n</gallery>",
    "<imagemap>\nFile:Map.png\n</imagemap>",
    "<pre>preformatted content</pre>",
    "<!-- a --> middle <!-- b -->",
    "<!-- [[NotALink]] -->",
    "before <includeonly>only when included</includeonly> after",
    "<noinclude>only on the template page</noinclude>",
    "before <translate>inside</translate> after",
    "<translate nowrap>text <nowiki>[[x]]</nowiki></translate>",
    "just plain text",
    /* ref with template content (regression: inner child was dropped) */
    "<ref>{{Cite web |title=Understanding The Meaning Behind The Anarchist Symbol: Breaking Down The Unconventional Ideals {{!}} ShunSpirit |url=https://shunspirit.com/article/anarchist-symbol-meaning |access-date=12 December 2025 |website=shunspirit.com}}</ref>",
    /* ref inside a file link caption */
    "[[File:Anarchy-symbol.svg|thumb|caption with '''bold'''<ref>{{Cite web |title=Some Title}}</ref>]]",
    /* inExt=true behavior should keep [[A|]] literal */
    "<ref>[[A|]]</ref>",
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

    /* stage 0 only (comment/ext detection) */
    size_t failed = run_parser_samples("comment_and_ext", samples, sizeof(samples) / sizeof(samples[0]) - 3, cfg, false, 0);

    /* last three samples must also survive all 10 stages */
    const char *full_stage_samples[] = {
        samples[sizeof(samples) / sizeof(samples[0]) - 2],
        samples[sizeof(samples) / sizeof(samples[0]) - 1],
        samples[sizeof(samples) / sizeof(samples[0]) - 3],
    };
    failed += run_parser_samples("comment_and_ext_full", full_stage_samples, 3, cfg, false, 10);

    config_free(cfg);
    return failed > 0 ? 1 : 0;
}
