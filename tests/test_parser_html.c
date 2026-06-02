#include <stdio.h>
#include <stdlib.h>
#include "wiki_cast/config.h"
#include "test_common.h"

#ifndef CONFIG_PATH
#define CONFIG_PATH "../config/default.json"
#endif

static const char *samples[] = {
    "plain <b>bold</b> text",
    "<span class=\"highlight\">inside</span> end",
    "line break<br/>here",
    "<b><i>bold italic</i></b>",
    "<foo>not a real tag</foo>",
    "== Heading with <span>span</span> ==",
    "<table><tr><td>cell</td></tr></table>",
    "<span id=\"x\" class=\"y\">text</span>",
    "<B>upper bold</B>",
    "<code>inline code</code> and <pre>block</pre>",
    "<b>unclosed bold",
    "<nowiki><b>not bold</b></nowiki>",
    "<b><i>bold</b></i>",
    "<abbr title=\"HyperText Markup Language\">HTML</abbr>",
    "<span></span>",
    "word<wbr/>break",
    /* HTML attribute parsing tests */
    "<div data-value='single quotes'>text</div>",
    "<a href=http://example.com>unquoted</a>",
    "<div class=no-quotes id=\"quoted\">mixed</div>",
    "<span  multiple   spaces=\"here\">content</span>",
    "<img alt=\"\" src=\"/path/to/img.png\" />",
    "<div onclick=\"alert('test')\">event</div>",
    "<meta data=\"itemprop\" content=\"x\">",
    "<link data=\"itemprop\" href=\"/x\">",
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

    size_t failed = run_parser_samples("html", samples, sizeof(samples) / sizeof(samples[0]), cfg, false, 2);
    config_free(cfg);
    return failed > 0 ? 1 : 0;
}
