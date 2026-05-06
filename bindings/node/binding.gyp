{
  "targets": [
    {
      "target_name": "wikiparser-node-c-tokenizer",
      "sources": [
        "src/addon.c",
        "../../src/accum.c",
        "../../src/build.c",
        "../../src/config.c",
        "../../src/converter.c",
        "../../src/parse.c",
        "../../src/parser/braces.c",
        "../../src/parser/comment_and_ext.c",
        "../../src/parser/converter.c",
        "../../src/parser/external_links.c",
        "../../src/parser/hr_and_double_underscore.c",
        "../../src/parser/html.c",
        "../../src/parser/link.c",
        "../../src/parser/links.c",
        "../../src/parser/list.c",
        "../../src/parser/magic_links.c",
        "../../src/parser/quotes.c",
        "../../src/parser/redirect.c",
        "../../src/parser/table.c",
        "../../src/string_util.c",
        "../../src/table_token.c",
        "../../src/thread_buffer.c",
        "../../src/tools/log.c",
        "../../src/td.c",
        "../../src/title.c",
        "../../src/token.c",
        "../../src/tr.c",
        "../../src/util/pcre_cache.c"
      ],
      "include_dirs": [ "../../include" ],
      "libraries": [ "-lpcre2-8", "-lcjson", "-licuuc", "-licudata" ]
    }
  ]
}
