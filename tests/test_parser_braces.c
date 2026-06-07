#include <stdio.h>
#include <stdlib.h>
#include "wiki_cast/config.h"
#include "test_common.h"

#ifndef CONFIG_PATH
#define CONFIG_PATH "../config/default.json"
#endif

static const char *samples[] = {
    "{{Template}}",
    "{{Template|arg}}",
    "{{Template|key=value}}",
    "{{Template|a|b|c}}",
    "{{Outer|{{Inner}}}}",
    "{{{arg}}}",
    "{{{arg|default}}}",
    "{{}}",
    "{{   }}",
    "{{!}}",
    "{{=}}",
    "before {{Template}} after",
    "{{A}} and {{B}} end",
    "{{ Template }}",
    "{{Template|}}",
    "{{ISBN|9781583228947}}",
    "= Heading =",
    "== Section ==",
    "====== Deep ======",
    "== Section ==   ",
    "== {{Template}} ==",
    "[[Main Page]]",
    "-{zh:漢字;zh-hans:汉字}-",
    /* Magic word variables (JS parity: config.variable list) */
    "{{PAGENAME}}",
    "{{CURRENTYEAR}}",
    "{{CURRENTMONTH}}",
    "{{CURRENTDAY}}",
    "see {{SITENAME}} here",
    "The year is {{#expr:{{CURRENTYEAR}}+1}}",
    "{{T|v=RFC 2119}}",
    "{{T|v=__NOTOC__}}",
    "{{subst:CURRENTYEAR}}",
    "{{safesubst:CURRENTYEAR}}",
    "{{#invoke:Foo|bar}}",
    "{{#invoke:Foo|bar|x=y}}",
    "{{Module:Foo}}",
    "{{Template:Bar}}",
    "{{User:Example}}",
    "{{:File:Example.jpg}}",
    "{{T|x=\n==H==\n}}",
    "{{A|\n=H=\n}}",
    "{{{a|<b>x</b>}}}",
    "{{{a|{{T}}}}}",
    "{{Navboxes\n|list=\n{{Libertarian socialism}}\n{{Libertarianism}}\n}}",
    "{{Refn|In ''Anarchism: From Theory to Practice'' (1970),{{Sfn|Guerin|1970|p=12}} anarchist historian [[Daniel Gu\xC3\xA9rin]] described it as a synonym for [[libertarian socialism]], and wrote that anarchism \"is really a synonym for socialism.\"{{Sfn|Arvidsson|2017}} In his many works on anarchism, historian [[Noam Chomsky]] describes anarchism, alongside [[libertarian Marxism]], as the [[libertarian]] wing of [[socialism]].{{Sfn|Otero|1994|p=617}}|group=nb}}",
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

    size_t failed = run_parser_samples("braces", samples, sizeof(samples) / sizeof(samples[0]), cfg, false, 1);
    config_free(cfg);
    return failed > 0 ? 1 : 0;
}
