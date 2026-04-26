cmake_minimum_required(VERSION 3.16)
project(wiki_tokenizer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── Dependencies ─────────────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(PCRE2   REQUIRED libpcre2-8)
pkg_check_modules(CJSON   REQUIRED libcjson)

include_directories(
    ${PCRE2_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# ── Library ────────────────────────────────────────────────────────────────── 
add_library(wikitokenizer STATIC
    src/token.c
    src/accum.c
    src/config.c
    src/string_util.c
    src/thread_buffer.c
    src/build.c
    src/title.c
    src/parse.c
    src/parser/redirect.c
    src/parser/comment_and_ext.c
    src/parser/braces.c
    src/parser/magic_links.c
    src/parser/html.c
    src/parser/table.c
    src/parser/hr_and_double_underscore.c
    src/parser/links.c
    src/parser/link.c
    src/parser/quotes.c
    src/parser/external_links.c
    src/parser/converter.c
    src/parser/list.c
    src/table_token.c
    src/tools/log.c
    src/tr.c
    src/td.c
    )

target_compile_options(wikitokenizer PRIVATE
    ${PCRE2_CFLAGS_OTHER}
    ${CJSON_CFLAGS_OTHER}
    -Wall -Wextra -Wno-unused-parameter
)

target_link_libraries(wikitokenizer
    ${PCRE2_LIBRARIES}
    ${CJSON_LIBRARIES}
)

# ── Test binaries ─────────────────────────────────────────────────────────────
enable_testing()

set(TEST_WIKI_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/node_modules/wikiparser-node/config/enwiki.json)

function(configure_parser_test target)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(${target} wikitokenizer)
    target_compile_definitions(${target} PRIVATE CONFIG_PATH="${TEST_WIKI_CONFIG}")
endfunction()

function(configure_parser_test_env test_name)
    set_tests_properties(${test_name} PROPERTIES
        ENVIRONMENT "WIKI_CONFIG=${TEST_WIKI_CONFIG}")
endfunction()

# Parser-specific unit tests (C)
add_executable(test_parser_redirect tests/test_parser_redirect.c)
configure_parser_test(test_parser_redirect)
add_test(NAME parser_redirect_c COMMAND test_parser_redirect)
configure_parser_test_env(parser_redirect_c)

add_executable(test_parser_comment_and_ext tests/test_parser_comment_and_ext.c)
configure_parser_test(test_parser_comment_and_ext)
add_test(NAME parser_comment_and_ext_c COMMAND test_parser_comment_and_ext)
configure_parser_test_env(parser_comment_and_ext_c)

add_executable(test_parser_braces tests/test_parser_braces.c)
configure_parser_test(test_parser_braces)
add_test(NAME parser_braces_c COMMAND test_parser_braces)
configure_parser_test_env(parser_braces_c)

add_executable(test_parser_html tests/test_parser_html.c)
configure_parser_test(test_parser_html)
add_test(NAME parser_html_c COMMAND test_parser_html)
configure_parser_test_env(parser_html_c)

add_executable(test_parser_table tests/test_parser_table.c)
configure_parser_test(test_parser_table)
add_test(NAME parser_table_c COMMAND test_parser_table)
configure_parser_test_env(parser_table_c)

add_executable(test_parser_hr_and_double_underscore tests/test_parser_hr_and_double_underscore.c)
configure_parser_test(test_parser_hr_and_double_underscore)
add_test(NAME parser_hr_and_double_underscore_c COMMAND test_parser_hr_and_double_underscore)
configure_parser_test_env(parser_hr_and_double_underscore_c)

add_executable(test_parser_links tests/test_parser_links.c)
configure_parser_test(test_parser_links)
add_test(NAME parser_links_c COMMAND test_parser_links)
configure_parser_test_env(parser_links_c)

add_executable(test_parser_quotes tests/test_parser_quotes.c)
configure_parser_test(test_parser_quotes)
add_test(NAME parser_quotes_c COMMAND test_parser_quotes)
configure_parser_test_env(parser_quotes_c)

add_executable(test_parser_external_links tests/test_parser_external_links.c)
configure_parser_test(test_parser_external_links)
add_test(NAME parser_external_links_c COMMAND test_parser_external_links)
configure_parser_test_env(parser_external_links_c)

add_executable(test_parser_magic_links tests/test_parser_magic_links.c)
configure_parser_test(test_parser_magic_links)
add_test(NAME parser_magic_links_c COMMAND test_parser_magic_links)
configure_parser_test_env(parser_magic_links_c)

add_executable(test_parser_list tests/test_parser_list.c)
configure_parser_test(test_parser_list)
add_test(NAME parser_list_c COMMAND test_parser_list)
configure_parser_test_env(parser_list_c)

add_executable(test_parser_converter tests/test_parser_converter.c)
configure_parser_test(test_parser_converter)
add_test(NAME parser_converter_c COMMAND test_parser_converter)
configure_parser_test_env(parser_converter_c)

add_executable(test_pipeline tests/test_pipeline.c)
configure_parser_test(test_pipeline)
add_test(NAME pipeline_c COMMAND test_pipeline)
configure_parser_test_env(pipeline_c)

add_executable(test_wikitext tests/test_wikitext.c)
configure_parser_test(test_wikitext)
add_test(NAME wikitext_c COMMAND test_wikitext
         ${TEST_WIKI_CONFIG}
         ${CMAKE_CURRENT_SOURCE_DIR}/tests/wikitext)
configure_parser_test_env(wikitext_c)

