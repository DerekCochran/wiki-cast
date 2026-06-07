#include <stdio.h>
#include <stdlib.h>
#include "wiki_cast/config.h"
#include "test_common.h"

#ifndef CONFIG_PATH
#define CONFIG_PATH "../config/default.json"
#endif

static const char *samples[] = {
    "This is a paragraph of plain text.",
    "#REDIRECT [[Target]] <!-- a comment -->",
    "{{Template|[[Page|label]]}}",
    "== {{PAGENAME}} ==\nContent.",
    "{|\n| [[Page|link]] || plain\n|}",
    "{|\n! Modern [[State of matter|state<br />of matter]]\n|}",
    "{{Outer|{{Inner|arg}}}}",
    "'''bold''' and ''[[Page|italic link]]''",
    "[[Page]] and [http://example.com external].",
    "See RFC 2119, [[Specification]] and http://example.com.",
    "* {{Template}}\n* plain item\n* [[Page]]",
    "== Before ==\n\n----\n\n== After ==",
    "== Relationship to surface [[bulk density]] ==",
    "Before __NOTOC__ after.",
    "{{Template|<!-- comment -->value}}",
    "<nowiki>[[not a link]] {{not a template}}</nowiki>",
    "Claim.<ref>{{Citation|title=Foo|year=2020}}</ref> More text.",
    "A<ref>''x'' efficacy is > <sub>2</sub>; ''y'' is > <sub>2</sub>.</ref>B",
    "A<ref>alpha<!--c--></ref>B",
    "A<ref>{{Citation|access-date= 29 June 2011<!--Added by DASHBot-->}}</ref>B",
    "{{T|v='''a\n''b'''}}",
    "<score raw=1 sound=1>\\relative c'' { x }</score>",
    "<ref>\n* a\n* b\n</ref>",
    "{{Blockquote|<poem><ref>x ''L'Etoile'' (as in ''H'')</ref></poem>}}",
    "<ref>a<!--c-->b ; -{zh-hans:简;zh-hant:繁;}-</ref>",
    "<ref>---- ; a<!--c-->b</ref>",
    "<ref>-{zh-hans:简;zh-hant:繁;}- ; a<!--c-->b</ref>",
    "<ref>* item ; a<!--c-->b</ref>",
    "{{Citation|title=Effect of land albedo, CO<sub>2</sub>, orography}}",
    "{{Refn|In ''Anarchism: From Theory to Practice'' (1970), historian [[Daniel Guérin]] describes [[libertarian socialism]].|group=nb}}",
    "{{Refn|In ''Anarchism: From Theory to Practice'' (1970),{{Sfn|Guérin|1970|p=12}} anarchist historian [[Daniel Guérin]] described it as a synonym for [[libertarian socialism]], and wrote that anarchism \"is really a synonym for socialism.\"{{Sfn|Arvidsson|2017}} In his many works on anarchism, historian [[Noam Chomsky]] describes anarchism, alongside [[libertarian Marxism]], as the [[libertarian]] wing of [[socialism]].{{Sfn|Otero|1994|p=617}}|group=nb}}",
    "'''Title''' (born [[1970]]) is a [[person]].\n\n== Career ==\n* [[Job A]]\n* [[Job B]]\n\n== References ==\n<references/>",
    "[[ẚ]]",
    "{{Wikisource-inline|list=\n** \"[[s:A Dictionary of the English Language/A|A]]\" in ''[[s:A Dictionary of the English Language|A Dictionary of the English Language]]'' by [[Samuel Johnson]]\n}}",
    "<gallery>\nFile:Achilles departure Eretria Painter CdM Paris 851.jpg|Achilles and the [[Nereid]] Cymothoe, Attic [[red-figure]] [[kantharos]] from [[Volci]] ([[Cabinet des Médailles]], Bibliothèque nationale, Paris)\n</gallery>",
    "<gallery>\nFile:Achilles departure Eretria Painter CdM Paris 851.jpg|Achilles and the [[Nereid]] Cymothoe\nFile:Akhilleus embassy Staatliche Antikensammlungen 8770.jpg|The embassy to Achilles, Attic red-figure [[hydria]]\n</gallery>",
    "(${{formatnum:{{Inflation|US|800|1861|r=-2}}}} in current dollars)",
    "<imagemap>\nFile:Emancipation proclamation.jpg|thumb|upright=1.25|''[[First Reading of the Emancipation Proclamation of President Lincoln]]''|alt=A dark-haired, bearded, middle-aged man holding documents is seated among seven other men.\npoly 269 892 254 775 193 738 [[Edwin M. Stanton|Edwin Stanton]]\n</imagemap>",
    "{{blockquote|<ref name=\"vra\">x [http://www.protectcivilrights.org/pdf/voting/AlabamaVRA.pdf ''Voting Rights in Alabama (1982–2006)''] {{Webarchive|url=x|date=y}} z</ref>}}",
    // Image: namespace (alias for File:) with nested link in caption
    "[[Image:Foo.jpg|caption [[Link]] text]]",
    "[[Image:Foo.jpg|upright|caption [[Link]] text]]",
    "{|\n| {{chset-cell1 | 123 U+007B: LEFT CURLY BRACKET | [[Left curly bracket|{]] | style=background:#ffffb2}}\n|}",
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

    size_t failed = run_parser_samples("pipeline", samples, sizeof(samples) / sizeof(samples[0]), cfg, false, 10);
    config_free(cfg);
    return failed > 0 ? 1 : 0;
}