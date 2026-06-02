#include <stdio.h>
#include <stdlib.h>
#include "wiki_cast/config.h"
#include "test_common.h"

#ifndef CONFIG_PATH
#define CONFIG_PATH "../config/default.json"
#endif

static const char *samples[] = {
    "See [[Main Page]] for details.",
    "[[Page|display text]]",
    "[[Page|]]",
    "[[:Category:Foo|label]]",
    "[[:File:Image.jpg|plain link]]",
    "[[Help:Contents]]",
    "[[Page#Section|section link]]",
    "[[File:Image.jpg]]",
    "[[File:Image.jpg|thumb|right|Caption text]]",
    "[[File:Image.jpg| thumb | upright=0.8 | Caption text]]",
    "[[File:water_reflectivity.jpg]]",
    "[[File:Image.jpg|thumbnail|right|Caption text]]",
    "[[File:Image.jpg|frameless|upright|center]]",
    "[[File:Image.jpg|link=Main Page|thumb|Caption]]",
    "[[File:Image.jpg|250px|left|Caption]]",
    "[[File:Image.jpg|alt=Example alt|thumb|Caption]]",
    "[[File:Image.jpg|[http://example.com Caption link]]]",
    "[[File:Image.jpg|thumb|[[Inner]] caption]]",
    "[[File:Image.jpg|thumb|[[Pierre-Joseph Proudhon]]]]",
    "[[Category:Example]]",
    "[[Category:Example|sort key]]",
    "[[#Section]]",
    "[[en:English article]]",
    "[[Page|{{Template}}]]",
    "[[Page A]] and [[Page B]] go here.",
    "[[Page]]s",
    "[[invalid link]]s or just [[valid]]",
    /* Anchor and image parameter tests */
    "[[Page#top|Back to top]]",
    "[[#References]]",
    "[[File:Image.jpg|120px|thumb|left|A [[link]] in caption]]",
    "[[File:Image.jpg|frameless|upright|center]]",
    "[[File:Image.jpg|none|border]]",
    "[[Category:Test#section|key]]",
    "[[en:Wikipedia#History]] with anchor",
    "[[File:Mardi&nbsp;Gras&nbsp;Mobile&nbsp;Order of Inca.jpg|thumb|left|upright|Mobile is the birthplace of Mardi Gras in the U.S.]]",
    /* Image (legacy) syntax tests */
    "[[Image:Justus Sustermans - Portrait of Galileo Galilei (Uffizi).jpg|left|thumb|upright|[[Galileo Galilei]] is often referred to as the father of [[modern astronomy]]. Portrait by [[Justus Sustermans]].]]",
    "[[Image:JKepler.jpg|right|thumb|upright|[[Johannes Kepler]], one of the fathers of [[modern astronomy]]]]",
    "[[Image:Foo.jpg|caption [[Link]] text]]",
    "[[Image:Foo.jpg|upright|caption [[Link]] text]]",
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

    size_t failed = run_parser_samples("links", samples, sizeof(samples) / sizeof(samples[0]), cfg, false, 5);
    config_free(cfg);
    return failed > 0 ? 1 : 0;
}
