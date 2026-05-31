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
#include "util/token_to_json.h"

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

static bool node_token_text_len(const Token *token, const char **text, size_t *len) {
    if (!token || token->child_count == 0 || !text || !len) return false;
    const Child *c = &token->children[0];
    if (!c->is_text || !c->text) return false;
    *text = c->text;
    *len = c->text_len;
    return true;
}

static void node_json_add_protocol_from_url(cJSON *root, const char *url, size_t url_len) {
    if (!root || !url || url_len == 0) return;
    for (size_t i = 0; i + 2 < url_len; i++) {
        if (url[i] == ':' && url[i + 1] == '/' && url[i + 2] == '/') {
            cJSON_AddItemToObject(root, "protocol", node_token_text_to_json_string(url, i + 3));
            return;
        }
    }
}

static void node_json_add_token_text_property(cJSON *root, const char *key, const Token *token) {
    if (!root || !key || !token) return;
    ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();
    if (!scratch) return;
    char *str = token_to_string(token, scratch);
    if (str) {
        cJSON_AddItemToObject(root, key, node_token_text_to_json_string(str, scratch->len));
    }
    wiki_thread_buf_release_scratch(scratch);
}

static bool node_child_token_at(const Token *token, size_t idx, const Token **out) {
    if (!token || !out || idx >= token->child_count) return false;
    const Child *c = &token->children[idx];
    if (c->is_text || !c->token) return false;
    *out = c->token;
    return true;
}

static bool node_child_text_at(const Token *token, size_t idx, const char **text, size_t *len) {
    if (!token || !text || !len || idx >= token->child_count) return false;
    const Child *c = &token->children[idx];
    if (!c->is_text || !c->text) return false;
    *text = c->text;
    *len = c->text_len;
    return true;
}

static void node_json_add_view_property(cJSON *root, const char *key, sz_string_view_t view) {
    if (!root || !key || !view.start) return;
    cJSON_AddItemToObject(root, key, node_token_text_to_json_string(view.start, view.length));
}

static void node_json_add_token_text_property_at(cJSON *root, const char *key, const Token *token, size_t idx) {
    const Token *child = NULL;
    if (node_child_token_at(token, idx, &child)) {
        node_json_add_token_text_property(root, key, child);
        return;
    }
    const char *text = NULL;
    size_t len = 0;
    if (node_child_text_at(token, idx, &text, &len)) {
        cJSON_AddItemToObject(root, key, node_token_text_to_json_string(text, len));
    }
}

static cJSON *node_json_text_child_as_string(const Token *token, size_t idx) {
    if (!token || idx >= token->child_count) return NULL;
    const Child *c = &token->children[idx];
    if (c->is_text && c->text) {
        return node_token_text_to_json_string(c->text, c->text_len);
    }
    if (!c->is_text && c->token) {
        const char *text = NULL;
        size_t len = 0;
        if (node_token_text_len(c->token, &text, &len)) {
            return node_token_text_to_json_string(text, len);
        }
    }
    return NULL;
}

static bool node_is_numeric_name(const char *name) {
    if (!name || !name[0]) return false;
    for (const char *p = name; *p; p++) {
        if (*p < '0' || *p > '9') return false;
    }
    return true;
}

static void node_add_image_parameter_dimensions(cJSON *root, const char *value, size_t len) {
    if (!root || !value || len == 0) return;
    size_t x = SIZE_MAX;
    for (size_t i = 0; i < len; i++) {
        if (value[i] == 'x' || value[i] == 'X') {
            x = i;
            break;
        }
    }
    if (x == SIZE_MAX) {
        cJSON_AddItemToObject(root, "width", node_token_text_to_json_string(value, len));
        return;
    }
    if (x > 0) {
        cJSON_AddItemToObject(root, "width", node_token_text_to_json_string(value, x));
    }
    if (x + 1 < len) {
        cJSON_AddItemToObject(root, "height", node_token_text_to_json_string(value + x + 1, len - x - 1));
    }
}

static void node_add_list_flags(cJSON *root, const Token *token) {
    if (!root || !token || token->child_count == 0) return;
    const char *text = NULL;
    size_t len = 0;
    const Token *marker_tok = NULL;
    if (node_child_text_at(token, 0, &text, &len)) {
        /* ok */
    } else if (node_child_token_at(token, 0, &marker_tok) && node_token_text_len(marker_tok, &text, &len)) {
        /* ok */
    } else {
        return;
    }

    int indent = 0;
    bool has_dd = false;
    bool has_dt = false;
    bool has_ul = false;
    bool has_ol = false;
    for (size_t i = 0; i < len; i++) {
        char ch = text[i];
        if (ch == ':' || ch == ';' || ch == '*' || ch == '#') {
            if (ch == ':') {
                indent++;
                has_dd = true;
            }
            else if (ch == ';') has_dt = true;
            else if (ch == '*') has_ul = true;
            else if (ch == '#') has_ol = true;
        }
    }
    cJSON_AddNumberToObject(root, "indent", indent);
    cJSON_AddBoolToObject(root, "dd", has_dd);
    cJSON_AddBoolToObject(root, "dt", has_dt);
    cJSON_AddBoolToObject(root, "ul", has_ul);
    cJSON_AddBoolToObject(root, "ol", has_ol);
}

static void node_add_converter_rule_fields(cJSON *root, const Token *token) {
    if (!root || !token) return;
    const Token *variant_tok = NULL;
    const Token *from_tok = NULL;
    for (size_t i = 0; i < token->child_count; i++) {
        const Token *child = NULL;
        if (!node_child_token_at(token, i, &child)) continue;
        if (child->subtype == TOKEN_SUBTYPE_CONVERTER_RULE_VARIANT) variant_tok = child;
        else if (child->subtype == TOKEN_SUBTYPE_CONVERTER_RULE_FROM) from_tok = child;
    }
    if (variant_tok) {
        node_json_add_token_text_property(root, "variant", variant_tok);
    }
    cJSON_AddBoolToObject(root, "unidirectional", from_tok != NULL);
    cJSON_AddBoolToObject(root, "bidirectional", variant_tok != NULL && from_tok == NULL);
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
        case TOKEN_PARAMETER:
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

    /* Subtype-specific JSON fields not represented directly in TokenType payload. */
    switch (token->subtype) {
        case TOKEN_SUBTYPE_IMAGE_PARAMETER:
            if (token->data.image_param.raw_syntax.start) {
                cJSON_AddStringToObject(root, "rawSyntax", token->data.image_param.raw_syntax.start);
            }
            node_json_add_token_text_property_at(root, "value", token, 0);
            if (token->name && strcmp(token->name, "width") == 0) {
                const Token *v = NULL;
                const char *value_text = NULL;
                size_t value_len = 0;
                if (node_child_token_at(token, 0, &v) && node_token_text_len(v, &value_text, &value_len)) {
                    node_add_image_parameter_dimensions(root, value_text, value_len);
                } else if (node_child_text_at(token, 0, &value_text, &value_len)) {
                    node_add_image_parameter_dimensions(root, value_text, value_len);
                }
            }
            break;

        case TOKEN_SUBTYPE_EXT_LINK:
            if (token->child_count > 0 && !token->children[0].is_text && token->children[0].token) {
                const Token *url_tok = token->children[0].token;
                const char *url_text = NULL;
                size_t url_len = 0;
                if (node_token_text_len(url_tok, &url_text, &url_len)) {
                    cJSON_AddItemToObject(root, "link", node_token_text_to_json_string(url_text, url_len));
                    node_json_add_protocol_from_url(root, url_text, url_len);
                }
            }
            if (token->child_count > 1 && !token->children[1].is_text && token->children[1].token) {
                node_json_add_token_text_property(root, "innerText", token->children[1].token);
            }
            break;

        case TOKEN_SUBTYPE_PARAMETER:
            if (token->name) cJSON_AddBoolToObject(root, "anon", node_is_numeric_name(token->name));
            node_json_add_token_text_property_at(root, "value", token, 1);
            break;

        case TOKEN_SUBTYPE_ARG:
            node_json_add_token_text_property_at(root, "default", token, 1);
            break;

        case TOKEN_SUBTYPE_TEMPLATE:
        case TOKEN_SUBTYPE_MAGIC_WORD: {
            size_t anon_count = 0;
            cJSON *seen = cJSON_CreateObject();
            bool dup = false;
            for (size_t i = 0; i < token->child_count; i++) {
                const Token *p = NULL;
                if (!node_child_token_at(token, i, &p)) continue;
                if (p->subtype != TOKEN_SUBTYPE_PARAMETER) continue;
                if (p->name && node_is_numeric_name(p->name)) anon_count++;
                if (seen && p->name && p->name[0]) {
                    cJSON *existing = cJSON_GetObjectItemCaseSensitive(seen, p->name);
                    if (existing) dup = true;
                    else cJSON_AddBoolToObject(seen, p->name, true);
                }
                if (p->subtype == TOKEN_SUBTYPE_PARAMETER && p->child_count > 0) {
                    const Token *k = NULL;
                    if (node_child_token_at(p, 0, &k) && k->subtype == TOKEN_SUBTYPE_PARAMETER_KEY) {
                        const Token *maybe_module = NULL;
                        if (node_child_token_at(k, 0, &maybe_module)) {
                            if (maybe_module->subtype == TOKEN_SUBTYPE_INVOKE_MODULE) {
                                node_json_add_token_text_property(root, "module", maybe_module);
                            } else if (maybe_module->subtype == TOKEN_SUBTYPE_INVOKE_FUNCTION) {
                                node_json_add_token_text_property(root, "function", maybe_module);
                            }
                        }
                    }
                }
            }
            if (seen) cJSON_Delete(seen);
            cJSON_AddNumberToObject(root, "anonCount", (double)anon_count);
            cJSON_AddBoolToObject(root, "duplication", dup);
            break;
        }

        case TOKEN_SUBTYPE_CONVERTER_FLAGS: {
            cJSON *arr = cJSON_CreateArray();
            if (arr) {
                for (size_t i = 0; i < token->child_count; i++) {
                    cJSON *item = node_json_text_child_as_string(token, i);
                    if (item) cJSON_AddItemToArray(arr, item);
                }
                cJSON_AddItemToObject(root, "flags", arr);
            }
            break;
        }

        case TOKEN_SUBTYPE_CONVERTER_RULE:
            node_add_converter_rule_fields(root, token);
            break;

        case TOKEN_SUBTYPE_CONVERTER:
            if (token->child_count > 1) {
                const Token *rule = NULL;
                if (node_child_token_at(token, 1, &rule) && rule->subtype == TOKEN_SUBTYPE_CONVERTER_RULE) {
                    node_add_converter_rule_fields(root, rule);
                }
            }
            break;

        case TOKEN_SUBTYPE_LIST:
        case TOKEN_SUBTYPE_DD:
            node_add_list_flags(root, token);
            break;

        case TOKEN_SUBTYPE_ONLYINCLUDE:
            node_json_add_token_text_property(root, "innerText", token);
            break;

        case TOKEN_SUBTYPE_PARAM_LINE:
            break;

        case TOKEN_SUBTYPE_IMAGEMAP_LINK:
            if (token->child_count > 1 && !token->children[1].is_text && token->children[1].token) {
                node_json_add_token_text_property(root, "link", token->children[1].token);
            }
            break;

        case TOKEN_SUBTYPE_EXT_LINK_URL:
        case TOKEN_SUBTYPE_FREE_EXT_LINK:
        case TOKEN_SUBTYPE_MAGIC_LINK: {
            const char *link_text = NULL;
            size_t link_len = 0;
            if (node_token_text_len(token, &link_text, &link_len)) {
                cJSON_AddItemToObject(root, "link", node_token_text_to_json_string(link_text, link_len));
                cJSON_AddItemToObject(root, "innerText", node_token_text_to_json_string(link_text, link_len));
                node_json_add_protocol_from_url(root, link_text, link_len);
            }
            break;
        }

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

static napi_value toInternalJson_wrapper(napi_env env, napi_callback_info info) {
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

    cJSON *json = token_to_json(token);
    if (!json) {
        napi_throw_error(env, NULL, "Failed to encode internal token JSON");
        return NULL;
    }

    char *json_text = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!json_text) {
        napi_throw_error(env, NULL, "Failed to stringify internal token JSON");
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
                "toInternalJson",
                NULL,
                toInternalJson_wrapper,
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
        napi_define_properties(env, js_token, 4, root_methods);
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