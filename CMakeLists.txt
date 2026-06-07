cmake_minimum_required(VERSION 3.16)
project(wiki_cast C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Default to Release for single-config generators unless user overrides.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Debug/Release/RelWithDebInfo)" FORCE)
endif()

# Gather C flags safely for sanitizer checks
set(_TOKENIZER_ALL_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG} ${CMAKE_C_FLAGS_RELEASE} ${CMAKE_C_FLAGS_RELWITHDEBINFO} ${CMAKE_C_FLAGS_MINSIZEREL}")

# ── Dependencies ─────────────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(CJSON   REQUIRED libcjson)
pkg_check_modules(ICU     REQUIRED icu-uc)

# ── Library ────────────────────────────────────────────────────────────────── 
# Build as shared library for use by Node.js addon
add_library(wiki_cast 
    src/token.c
    src/accum.c
    src/config.c
    src/string_util.c
    src/build.c
    src/title.c
    src/parse.c
    src/parser/redirect.c
    src/parser/comment_and_ext.c
    src/parser/braces.c
    src/parser/brace_pat.c
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
    src/tr.c
    src/td.c
    src/util/env_cache.c
    src/util/callback_parser.c
    src/util/wiki_parser_rules.c
    src/util/log.c
    src/util/thread_buffer.c
    src/util/token_to_json.c
)

# Modern target-scoped include directories
target_include_directories(wiki_cast PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(wiki_cast PRIVATE
    ${CJSON_INCLUDE_DIRS}
    ${ICU_INCLUDE_DIRS}
    src/include
)

# Modern target-scoped compile options
target_compile_options(wiki_cast PRIVATE
    -std=gnu17
    -fPIC
    -march=native 
    -mtune=native
    -Wall -Wextra -Wno-unused-parameter
    ${CJSON_CFLAGS_OTHER}
    ${ICU_CFLAGS_OTHER}
    $<$<CONFIG:Release>:-O3> 
    $<$<CONFIG:RelWithDebInfo>:-O3>
)

# Modern target-scoped definitions
target_compile_definitions(wiki_cast PRIVATE _DEFAULT_SOURCE)
if(_TOKENIZER_ALL_C_FLAGS MATCHES "(^|[ ;])-fsanitize=undefined([ ;]|$)")
    target_compile_definitions(wiki_cast PRIVATE SZ_USE_MISALIGNED_LOADS=0)
endif()

# Interprocedural Optimization (LTO)
set_target_properties(wiki_cast PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
if(_TOKENIZER_ALL_C_FLAGS MATCHES "(^|[ ;])-fsanitize=thread([ ;]|$)")
    set_target_properties(wiki_cast PROPERTIES INTERPROCEDURAL_OPTIMIZATION FALSE)
endif()

target_link_libraries(wiki_cast PRIVATE
    ${CJSON_LIBRARIES}
    ${ICU_LIBRARIES}
)

# ── Installation and Export (CRITICAL FOR RELEASES) ──────────────────────────
include(GNUInstallDirs)

# Install the library binary and assign it to an Export set
install(TARGETS wiki_cast
    EXPORT wiki_castTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install public headers (Assuming your public headers are in /include)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Generate and install the CMake target files so downstreams can find it
install(EXPORT wiki_castTargets
    FILE wiki_castTargets.cmake
    NAMESPACE wiki_cast::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wiki_cast
)

# Create a minimal config file for find_package()
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/wiki_castConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/wiki_castConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wiki_cast
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/wiki_castConfig.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wiki_cast
)

# ── Test binaries ─────────────────────────────────────────────────────────────
enable_testing()

set(TEST_WIKI_CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/config/enwiki.json)

function(configure_parser_test target)
    target_link_libraries(${target} PRIVATE wiki_cast)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/include)
    target_compile_definitions(${target} PRIVATE CONFIG_PATH="${TEST_WIKI_CONFIG}")
endfunction()

function(configure_parser_test_env test_name)
    set_tests_properties(${test_name} PROPERTIES ENVIRONMENT "WIKI_CONFIG=${TEST_WIKI_CONFIG}")
endfunction()

# Macro-style helper to reduce test boilerplate
macro(add_wiki_test name_suffix file_src)
    add_executable(test_parser_${name_suffix} tests/${file_src}.c)
    configure_parser_test(test_parser_${name_suffix})
    add_test(NAME parser_${name_suffix}_c COMMAND test_parser_${name_suffix})
    configure_parser_test_env(parser_${name_suffix}_c)
endmacro()

add_wiki_test(redirect test_parser_redirect)
add_wiki_test(comment_and_ext test_parser_comment_and_ext)
add_wiki_test(braces test_parser_braces)
add_wiki_test(html test_parser_html)
add_wiki_test(table test_parser_table)
add_wiki_test(hr_and_double_underscore test_parser_hr_and_double_underscore)
add_wiki_test(links test_parser_links)
add_wiki_test(quotes test_parser_quotes)
add_wiki_test(external_links test_parser_external_links)
add_wiki_test(magic_links test_parser_magic_links)
add_wiki_test(list test_parser_list)

# Custom configs for specialized tests
add_executable(test_parser_converter tests/test_parser_converter.c)
configure_parser_test(test_parser_converter)
add_test(NAME parser_converter_c COMMAND test_parser_converter)

add_executable(test_pipeline tests/test_pipeline.c)
configure_parser_test(test_pipeline)
add_test(NAME pipeline_c COMMAND test_pipeline)
configure_parser_test_env(pipeline_c)

add_executable(test_wikitext tests/test_wikitext.c)
configure_parser_test(test_wikitext)
add_test(NAME wikitext_c COMMAND test_wikitext ${TEST_WIKI_CONFIG} ${CMAKE_CURRENT_SOURCE_DIR}/tests/wikitext)
configure_parser_test_env(wikitext_c)

# ── Node.js Addon Binding ────────────────────────────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(NODE_GYP_CONFIG_FLAG "--debug")
else()
    set(NODE_GYP_CONFIG_FLAG "--release")
endif()

add_custom_target(node_binding ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Building Node native addon ${NODE_GYP_CONFIG_FLAG}"
    COMMAND ${CMAKE_COMMAND} -E env npx node-gyp rebuild ${NODE_GYP_CONFIG_FLAG}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bindings/node
    DEPENDS wiki_cast
)