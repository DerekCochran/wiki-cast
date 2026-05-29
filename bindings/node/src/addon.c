#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <node_api.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <cjson/cJSON.h>

/* extern_tokenizer headers */
#include "parse.h"
#include "token.h"
#include "config.h"

// Cache for config
static char* cached_config_path = NULL;
static ParserConfig* cached_config = NULL;

static double now_ms_monotonic(void) {
#ifdef CLOCK_MONOTONIC
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
    }
#endif
    struct timespec ts_fallback;
    timespec_get(&ts_fallback, TIME_UTC);
    return (double)ts_fallback.tv_sec * 1000.0 + (double)ts_fallback.tv_nsec / 1000000.0;
}

static double now_ms_thread_cpu(void) {
#ifdef CLOCK_THREAD_CPUTIME_ID
    struct timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0) {
        return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
    }
#endif
    return -1.0;
}

static void token_finalizer(napi_env env, void *finalize_data, void *finalize_context) {
    Token *token = (Token *)finalize_data;
    token_free(token);
    return;
}

static napi_value free_wrapper(napi_env env, napi_callback_info info) {
    napi_value this_arg;
    napi_status status = napi_get_cb_info(env, info, NULL, NULL, &this_arg, NULL);
    if (status != napi_ok) {
        return NULL;
    }

    void *wrapped = NULL;
    status = napi_remove_wrap(env, this_arg, &wrapped);

    bool freed = false;
    if (status == napi_ok && wrapped) {
        token_free((Token *)wrapped);
        freed = true;
    }

    napi_value out;
    napi_get_boolean(env, freed, &out);
    return out;
}

static napi_value toString_wrapper(napi_env env, napi_callback_info info) {
    napi_value this_arg;
    napi_status status = napi_get_cb_info(env, info, NULL, NULL, &this_arg, NULL);
    if (status != napi_ok) {
        return NULL;
    }

    void *wrapped = NULL;
    status = napi_unwrap(env, this_arg, &wrapped);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to unwrap root token");
        return NULL;
    }

    Token *token = (Token *)wrapped;
    if (!token) {
        napi_throw_error(env, NULL, "Token pointer is NULL");
        return NULL;
    }

    ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();
    char *str = token_to_string(token, scratch);
    
    if (!str) {
        wiki_thread_buf_release_scratch(scratch);
        napi_throw_error(env, NULL, "Internal conversion to string failed");
        return NULL;
    }

    napi_value result;
    status = napi_create_string_utf8(env, str, scratch->len, &result);
    
    wiki_thread_buf_release_scratch(scratch);
    
    if (status != napi_ok) return NULL;
    return result; 
}

static cJSON *node_token_text_to_json_string(const char *text, size_t text_len) {
    if (!text || text_len == 0) {
        return cJSON_CreateString("");
    }
    char *tmp = (char *)malloc(text_len + 1);
    if (!tmp) return cJSON_CreateString("");
    memcpy(tmp, text, text_len);
    tmp[text_len] = '\0';
    cJSON *s = cJSON_CreateString(tmp);
    free(tmp);
    return s ? s : cJSON_CreateString("");
}

static void node_json_add_cstr_to_object(cJSON *root, const char *key, const char *value) {
    if (!root || !key || !value) return;
    cJSON_AddStringToObject(root, key, value);
}

static cJSON *token_to_node_json(const Token *token) {
    if (!token) return NULL;

    cJSON *root = cJSON_CreateObject();
    sz_string_view_t subtype_name = token_subtype_name(token->subtype);

    cJSON_AddItemToObject(root, "type", node_token_text_to_json_string(subtype_name.start ? subtype_name.start : "", subtype_name.length));
    if (token->name) node_json_add_cstr_to_object(root, "name", token->name);

    switch (token->type) {
        case TOKEN_HEADING:
            cJSON_AddNumberToObject(root, "level", token->data.heading.level);
            break;

        case TOKEN_COMMENT:
            cJSON_AddBoolToObject(root, "closed", token->data.comment.closed);
            break;

        case TOKEN_HTML:
            cJSON_AddBoolToObject(root, "selfClosing", token->data.html.self_closing);
            cJSON_AddBoolToObject(root, "closing", token->data.html.closing);
            if (token->data.html.orig_tag.start) {
                cJSON_AddStringToObject(root, "origTag", token->data.html.orig_tag.start);
            }
            break;

        case TOKEN_TD:
            if (token->data.td.inner_syntax.start) {
                cJSON_AddStringToObject(root, "innerSyntax", token->data.td.inner_syntax.start);
            }
            break;

        case TOKEN_DOUBLE_UNDERSCORE:
            cJSON_AddBoolToObject(root, "caseSensitive", token->data.dunder.case_sensitive);
            cJSON_AddBoolToObject(root, "fullwidth", token->data.dunder.fullwidth);
            break;

        case TOKEN_QUOTE:
            cJSON_AddBoolToObject(root, "bold", token->data.quote.bold);
            cJSON_AddBoolToObject(root, "italic", token->data.quote.italic);
            break;

        case TOKEN_REDIRECT:
            if (token->data.redirect.display.start) cJSON_AddStringToObject(root, "display", token->data.redirect.display.start);
            break;

        case TOKEN_EXT:
            if (token->data.ext.name.start) cJSON_AddStringToObject(root, "extName", token->data.ext.name.start);
            if (token->data.ext.attr.start) cJSON_AddStringToObject(root, "extAttr", token->data.ext.attr.start);
            if (token->data.ext.inner.start) cJSON_AddStringToObject(root, "extInner", token->data.ext.inner.start);
            if (token->data.ext.closing.start) cJSON_AddStringToObject(root, "extClosing", token->data.ext.closing.start);
            cJSON_AddBoolToObject(root, "extSelfClosing", token->data.ext.self_closing);
            break;

        case TOKEN_NOINCLUDE:
        case TOKEN_INCLUDE:
        case TOKEN_ONLYINCLUDE:
        case TOKEN_TRANSLATE:
            if (token->data.include.tag.start) cJSON_AddStringToObject(root, "tag", token->data.include.tag.start);
            if (token->data.include.attr.start) cJSON_AddStringToObject(root, "includeAttr", token->data.include.attr.start);
            if (token->data.include.inner.start) cJSON_AddStringToObject(root, "includeInner", token->data.include.inner.start);
            if (token->data.include.closing.start) cJSON_AddStringToObject(root, "includeClosing", token->data.include.closing.start);
            break;

        case TOKEN_EXT_ATTR:
            if (token->data.ext_attr.equal.start) cJSON_AddStringToObject(root, "equal", token->data.ext_attr.equal.start);
            {
                char qo[2] = {token->data.ext_attr.quote_open, '\0'};
                char qc[2] = {token->data.ext_attr.quote_close, '\0'};
                cJSON_AddStringToObject(root, "quoteOpen", qo);
                cJSON_AddStringToObject(root, "quoteClose", qc);
            }
            break;

        case TOKEN_PARAMETER:
            if (token->data.image_param.raw_syntax.start) cJSON_AddStringToObject(root, "rawSyntax", token->data.image_param.raw_syntax.start);
            break;

        case TOKEN_MAGIC_LINK:
        case TOKEN_EXT_LINK:
            if (token->data.ext_link.space.start) cJSON_AddStringToObject(root, "space", token->data.ext_link.space.start);
            break;

        case TOKEN_LINK:
        case TOKEN_FILE:
        case TOKEN_CATEGORY:
            cJSON_AddBoolToObject(root, "magicPipe", token->data.link.magic_pipe);
            break;

        case TOKEN_TRANSCLUDE:
        case TOKEN_ARG:
            if (token->data.transclude.modifier.start) cJSON_AddStringToObject(root, "modifier", token->data.transclude.modifier.start);
            break;

        default:
            break;
    }

    if (token->child_count > 0) {
        cJSON *children_arr = cJSON_AddArrayToObject(root, "childNodes");
        for (size_t i = 0; i < token->child_count; i++) {
            Child *child = &token->children[i];
            if (child->is_text) {
                cJSON_AddItemToArray(children_arr, node_token_text_to_json_string(child->text, child->text_len));
            } else {
                cJSON_AddItemToArray(children_arr, token_to_node_json(child->token));
            }
        }
    }

    return root;
}

static napi_value toJson_wrapper(napi_env env, napi_callback_info info) {
    napi_value this_arg;
    napi_status status = napi_get_cb_info(env, info, NULL, NULL, &this_arg, NULL);
    if (status != napi_ok) {
        return NULL;
    }

    void *wrapped = NULL;
    status = napi_unwrap(env, this_arg, &wrapped);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to unwrap root token");
        return NULL;
    }

    Token *token = (Token *)wrapped;
    if (!token) {
        napi_throw_error(env, NULL, "Token pointer is NULL");
        return NULL;
    }

    cJSON *json = token_to_node_json(token);
    if (!json) {
        napi_throw_error(env, NULL, "Failed to encode token as JSON");
        return NULL;
    }

    char *json_text = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!json_text) {
        napi_throw_error(env, NULL, "Failed to stringify token JSON");
        return NULL;
    }

    napi_value result;
    status = napi_create_string_utf8(env, json_text, NAPI_AUTO_LENGTH, &result);
    cJSON_free(json_text);
    if (status != napi_ok) {
        return NULL;
    }
    return result;
}

static ParserConfig* get_token_config_json(napi_env env, napi_value token) {
    napi_value config_val;
    napi_status status;

    // Directly get the .config property from the object
    status = napi_get_named_property(env, token, "config", &config_val);
    if (status != napi_ok) return NULL;

    // Check if it's actually a string
    napi_valuetype type;
    napi_typeof(env, config_val, &type);
    if (type != napi_string) {
        napi_throw_error(env, NULL, "Property 'config' must be a string");
        return NULL;
    }

    // Extract the string content
    size_t path_len;
    napi_get_value_string_utf8(env, config_val, NULL, 0, &path_len);
    
    char *path = malloc(path_len + 1);
    if (!path) {
        napi_throw_error(env, NULL, "Memory allocation failed");
        return NULL;
    }
    napi_get_value_string_utf8(env, config_val, path, path_len + 1, &path_len);

    // Check if the path matches the cached path
    if (cached_config_path && strcmp(path, cached_config_path) == 0) {
        // Path matches, return cached config
        free(path);
        return cached_config;
    }
    
    // Path doesn't match or no cached config, load new config
    ParserConfig* cfg = config_load_file(path);
    if (cfg) {
        // Free old cached config if exists
        if (cached_config) {
            config_free(cached_config);
            free(cached_config_path);
        }
        cached_config = cfg;
        cached_config_path = strdup(path);
        free(path);
        if (!cached_config_path) {
            // strdup failed
            config_free(cfg);
            cached_config = NULL;
            napi_throw_error(env, NULL, "Memory allocation failed");
            return NULL;
        }
    } else {
        napi_throw_error(env, NULL, "Failed to load config file");
        free(path);
        return NULL;
    }
    
    return cfg;
}

/* Create the JS root wrapper; token data is accessed via root methods. */
static napi_value token_to_js(napi_env env, const Token *token, bool wrap_root) {
    napi_value js_token;
    napi_create_object(env, &js_token);

    if (wrap_root) {
        napi_wrap(env, js_token, (void *)token, token_finalizer, NULL, NULL);
    }

    if (wrap_root) {
        napi_property_descriptor root_methods[] = {
            {
                "toString",
                NULL,
                toString_wrapper,
                NULL,
                NULL,
                NULL,
                napi_default,
                NULL
            },
            {
                "toJson",
                NULL,
                toJson_wrapper,
                NULL,
                NULL,
                NULL,
                napi_default,
                NULL
            },
            {
                "_freeNative",
                NULL,
                free_wrapper,
                NULL,
                NULL,
                NULL,
                napi_default,
                NULL
            }
        };
        napi_define_properties(env, js_token, 3, root_methods);
    }

    return js_token;
}

/**
 * The main N-API entry point for the `parse` function.
 * @param args[0] Uint8Array (the buffer)
 * @param args[1] Object { config: ... }
 */
static napi_value parse(napi_env env, napi_callback_info info) {
    size_t argc = 2;
    napi_value args[2];
    napi_value this_arg;
    napi_status status = napi_get_cb_info(env, info, &argc, args, &this_arg, NULL);
    if (status != napi_ok || argc < 1) {
        napi_throw_error(env, NULL, "Invalid arguments. Expected (Buffer, {config: ...})");
        return NULL;
    }

    napi_value buffer_typedarray = args[0];
    
    // 1. Declare the correct enum type for TypedArrays
    napi_typedarray_type ta_type;
    napi_value buffer;
    size_t byte_length;
    size_t byte_offset;
    void* data;

    // 2. Get info. Note: We use &ta_type, NOT a napi_valuetype.
    status = napi_get_typedarray_info(
        env, 
        buffer_typedarray, 
        &ta_type, 
        &byte_length, 
        &data, 
        &buffer, 
        &byte_offset
    );

    // 3. Check if the call succeeded and if it's the right kind of array
    if (status != napi_ok || ta_type != napi_uint8_array) {
        napi_throw_error(env, NULL, "Expected Uint8Array (Buffer)");
        return NULL;
    }

    // The wikitext is a view into the buffer starting at the offset
    const char *wikitext = (const char *)data;

    /* Load parser config from the calling JS Token instance. */
    ParserConfig* cfg = get_token_config_json(env, this_arg);
    if (!cfg) {
        // Error already thrown by get_token_config_json
        return NULL;
    }

    // 3. Call the C parser
    //double parse_t0_ms = now_ms_monotonic();
    //double parse_cpu_t0_ms = now_ms_thread_cpu();
    Token *root = wiki_parse(wikitext, byte_length, cfg, false, 10);
    //double parse_ms = now_ms_monotonic() - parse_t0_ms;
    //double parse_cpu_ms = -1.0;
    //double parse_cpu_t1_ms = now_ms_thread_cpu();
    // if (parse_cpu_t0_ms >= 0.0 && parse_cpu_t1_ms >= 0.0) {
    //     parse_cpu_ms = parse_cpu_t1_ms - parse_cpu_t0_ms;
    // }
    // if (parse_cpu_ms >= 0.0) {
    //     fprintf(stderr, "[wiki-cast.node] wiki_parse elapsed_ms=%.3f cpu_thread_ms=%.3f bytes=%zu\n",
    //             parse_ms, parse_cpu_ms, byte_length);
    // } else {
    //     fprintf(stderr, "[wiki-cast.node] wiki_parse elapsed_ms=%.3f bytes=%zu\n", parse_ms, byte_length);
    // }
    
    if (root == NULL) {
        napi_throw_error(env, NULL, "Wiki parse failed.");
        return NULL;
    }

    // 4. Convert the Token tree to a JS object
    napi_value js_root = token_to_js(env, root, true);

    return js_root;
}

/**
 * N-API Module Initialization
 */
napi_value Init(napi_env env, napi_value exports) {
    napi_value initial_config_val;
    napi_status status = napi_create_string_utf8(env, "", 0, &initial_config_val);
    if (status != napi_ok) return NULL;

    napi_property_descriptor desc[] = {
        {
            .utf8name = "parse",
            .method = parse,
            .attributes = napi_default
        },
        {
            .utf8name = "config",
            .value = initial_config_val,
            .attributes = (napi_property_attributes)(napi_writable | napi_enumerable | napi_configurable)
        }
    };
    napi_define_properties(env, exports, 2, desc);

    return exports;
}
NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)