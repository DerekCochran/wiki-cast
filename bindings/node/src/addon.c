#include <node_api.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* extern_tokenizer headers */
#include "parse.h"
#include "token.h"
#include "config.h"

// Cache for config
static char* cached_config_path = NULL;
static ParserConfig* cached_config = NULL;

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
    void *data;
    napi_get_cb_info(env, info, NULL, NULL, NULL, &data);
    
    Token *token = (Token *)data;
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
    napi_status status = napi_create_string_utf8(env, str, scratch->len, &result);
    
    wiki_thread_buf_release_scratch(scratch);
    
    if (status != napi_ok) return NULL;
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

/**
 * Recursively converts a Token tree into a JavaScript object.
 * This mirrors the structure of the JS Token class.
 */
static napi_value token_to_js(napi_env env, const Token *token, bool wrap_root) {
    napi_value js_token;
    napi_create_object(env, &js_token);

    if (wrap_root) {
        napi_wrap(env, js_token, (void *)token, token_finalizer, NULL, NULL);
    }

    // Attach toString directly to this token object
    napi_property_descriptor toString_desc = {
        "toString",
        NULL,
        toString_wrapper,
        NULL,
        NULL,
        NULL,
        napi_default,
        (void *)token
    };
    napi_define_properties(env, js_token, 1, &toString_desc);

    if (wrap_root) {
        napi_property_descriptor free_desc = {
            "_freeNative",
            NULL,
            free_wrapper,
            NULL,
            NULL,
            NULL,
            napi_default,
            NULL
        };
        napi_define_properties(env, js_token, 1, &free_desc);
    }

    // type
    napi_value type_val;
    napi_create_string_utf8(env, token->type_name, NAPI_AUTO_LENGTH, &type_val);
    napi_set_named_property(env, js_token, "type", type_val);

    // name (optional)
    if (token->name) {
        napi_value name_val;
        napi_create_string_utf8(env, token->name, NAPI_AUTO_LENGTH, &name_val);
        napi_set_named_property(env, js_token, "name", name_val);
    }

    // children
    if (token->child_count > 0) {
        napi_value children_array;
        napi_create_array_with_length(env, token->child_count, &children_array);
        for (size_t i = 0; i < token->child_count; i++) {
            Child *child = &token->children[i];
            if (child->is_text) {
                napi_value text_val;
                napi_create_string_utf8(env, child->text, child->text_len, &text_val);
                napi_set_element(env, children_array, i, text_val);
            } else {
                napi_value child_js = token_to_js(env, child->token, false);
                napi_set_element(env, children_array, i, child_js);
            }
        }
        napi_set_named_property(env, js_token, "childNodes", children_array);
    }

    // Type-specific properties
    switch (token->type) {
        case TOKEN_HEADING: {
            napi_value level_val;
            napi_create_int32(env, token->data.heading.level, &level_val);
            napi_set_named_property(env, js_token, "level", level_val);
            break;
        }
        case TOKEN_COMMENT: {
            napi_value closed_val;
            napi_get_boolean(env, token->data.comment.closed, &closed_val);
            napi_set_named_property(env, js_token, "closed", closed_val);
            break;
        }
        case TOKEN_HTML: {
            napi_value self_closing_val;
            napi_get_boolean(env, token->data.html.self_closing, &self_closing_val);
            napi_set_named_property(env, js_token, "selfClosing", self_closing_val);
            napi_value closing_val;
            napi_get_boolean(env, token->data.html.closing, &closing_val);
            napi_set_named_property(env, js_token, "closing", closing_val);
            if (token->data.html.orig_tag) {
                napi_value orig_tag_val;
                napi_create_string_utf8(env, token->data.html.orig_tag, NAPI_AUTO_LENGTH, &orig_tag_val);
                napi_set_named_property(env, js_token, "origTag", orig_tag_val);
            }
            break;
        }
        case TOKEN_TD: {
            if (token->data.td.inner_syntax) {
                napi_value inner_syntax_val;
                napi_create_string_utf8(env, token->data.td.inner_syntax, NAPI_AUTO_LENGTH, &inner_syntax_val);
                napi_set_named_property(env, js_token, "innerSyntax", inner_syntax_val);
            }
            break;
        }
        case TOKEN_DOUBLE_UNDERSCORE: {
            napi_value case_sensitive_val;
            napi_get_boolean(env, token->data.dunder.case_sensitive, &case_sensitive_val);
            napi_set_named_property(env, js_token, "caseSensitive", case_sensitive_val);
            napi_value fullwidth_val;
            napi_get_boolean(env, token->data.dunder.fullwidth, &fullwidth_val);
            napi_set_named_property(env, js_token, "fullwidth", fullwidth_val);
            break;
        }
        case TOKEN_QUOTE: {
            napi_value bold_val;
            napi_get_boolean(env, token->data.quote.bold, &bold_val);
            napi_set_named_property(env, js_token, "bold", bold_val);
            napi_value italic_val;
            napi_get_boolean(env, token->data.quote.italic, &italic_val);
            napi_set_named_property(env, js_token, "italic", italic_val);
            break;
        }
        case TOKEN_REDIRECT: {
            // if (token->data.redirect.pre) {
            //     napi_value pre_val;
            //     napi_create_string_utf8(env, token->data.redirect.pre, NAPI_AUTO_LENGTH, &pre_val);
            //     napi_set_named_property(env, js_token, "pre", pre_val);
            // }
            // if (token->data.redirect.post) {
            //     napi_value post_val;
            //     napi_create_string_utf8(env, token->data.redirect.post, NAPI_AUTO_LENGTH, &post_val);
            //     napi_set_named_property(env, js_token, "post", post_val);
            // }
            // if (token->data.redirect.link) {
            //     napi_value link_val;
            //     napi_create_string_utf8(env, token->data.redirect.link, NAPI_AUTO_LENGTH, &link_val);
            //     napi_set_named_property(env, js_token, "link", link_val);
            // }
            if (token->data.redirect.display) {
                napi_value display_val;
                napi_create_string_utf8(env, token->data.redirect.display, NAPI_AUTO_LENGTH, &display_val);
                napi_set_named_property(env, js_token, "display", display_val);
            }
            break;
        }
        case TOKEN_EXT: {
            if (token->data.ext.name) {
                napi_value name_val;
                napi_create_string_utf8(env, token->data.ext.name, NAPI_AUTO_LENGTH, &name_val);
                napi_set_named_property(env, js_token, "extName", name_val);
            }
            if (token->data.ext.attr) {
                napi_value attr_val;
                napi_create_string_utf8(env, token->data.ext.attr, NAPI_AUTO_LENGTH, &attr_val);
                napi_set_named_property(env, js_token, "extAttr", attr_val);
            }
            if (token->data.ext.inner) {
                napi_value inner_val;
                napi_create_string_utf8(env, token->data.ext.inner, NAPI_AUTO_LENGTH, &inner_val);
                napi_set_named_property(env, js_token, "extInner", inner_val);
            }
            if (token->data.ext.closing) {
                napi_value closing_val;
                napi_create_string_utf8(env, token->data.ext.closing, NAPI_AUTO_LENGTH, &closing_val);
                napi_set_named_property(env, js_token, "extClosing", closing_val);
            }
            napi_value self_closing_val;
            napi_get_boolean(env, token->data.ext.self_closing, &self_closing_val);
            napi_set_named_property(env, js_token, "extSelfClosing", self_closing_val);
            break;
        }
        case TOKEN_NOINCLUDE:
        case TOKEN_INCLUDE:
        case TOKEN_ONLYINCLUDE:
        case TOKEN_TRANSLATE: {
            if (token->data.include.tag) {
                napi_value tag_val;
                napi_create_string_utf8(env, token->data.include.tag, NAPI_AUTO_LENGTH, &tag_val);
                napi_set_named_property(env, js_token, "tag", tag_val);
            }
            if (token->data.include.attr) {
                napi_value attr_val;
                napi_create_string_utf8(env, token->data.include.attr, NAPI_AUTO_LENGTH, &attr_val);
                napi_set_named_property(env, js_token, "includeAttr", attr_val);
            }
            if (token->data.include.inner) {
                napi_value inner_val;
                napi_create_string_utf8(env, token->data.include.inner, NAPI_AUTO_LENGTH, &inner_val);
                napi_set_named_property(env, js_token, "includeInner", inner_val);
            }
            if (token->data.include.closing) {
                napi_value closing_val;
                napi_create_string_utf8(env, token->data.include.closing, NAPI_AUTO_LENGTH, &closing_val);
                napi_set_named_property(env, js_token, "includeClosing", closing_val);
            }
            break;
        }
        case TOKEN_EXT_ATTR: {
            if (token->data.ext_attr.equal) {
                napi_value equal_val;
                napi_create_string_utf8(env, token->data.ext_attr.equal, NAPI_AUTO_LENGTH, &equal_val);
                napi_set_named_property(env, js_token, "equal", equal_val);
            }
            napi_value quote_open_val;
            char quote_open_str[2] = {token->data.ext_attr.quote_open, '\0'};
            napi_create_string_utf8(env, quote_open_str, NAPI_AUTO_LENGTH, &quote_open_val);
            napi_set_named_property(env, js_token, "quoteOpen", quote_open_val);
            napi_value quote_close_val;
            char quote_close_str[2] = {token->data.ext_attr.quote_close, '\0'};
            napi_create_string_utf8(env, quote_close_str, NAPI_AUTO_LENGTH, &quote_close_val);
            napi_set_named_property(env, js_token, "quoteClose", quote_close_val);
            break;
        }
        case TOKEN_PARAMETER: {
            if (token->data.image_param.raw_syntax) {
                napi_value raw_syntax_val;
                napi_create_string_utf8(env, token->data.image_param.raw_syntax, NAPI_AUTO_LENGTH, &raw_syntax_val);
                napi_set_named_property(env, js_token, "rawSyntax", raw_syntax_val);
            }
            break;
        }
        case TOKEN_EXT_LINK:
        case TOKEN_MAGIC_LINK: {
            if (token->data.ext_link.space) {
                napi_value space_val;
                napi_create_string_utf8(env, token->data.ext_link.space, NAPI_AUTO_LENGTH, &space_val);
                napi_set_named_property(env, js_token, "space", space_val);
            }
            break;
        }
        case TOKEN_LINK:
        case TOKEN_FILE:
        case TOKEN_CATEGORY: {
            napi_value magic_pipe_val;
            napi_get_boolean(env, token->data.link.magic_pipe, &magic_pipe_val);
            napi_set_named_property(env, js_token, "magicPipe", magic_pipe_val);
            break;
        }
        case TOKEN_TRANSCLUDE:
        case TOKEN_ARG: {
            if (token->data.transclude.modifier) {
                napi_value modifier_val;
                napi_create_string_utf8(env, token->data.transclude.modifier, NAPI_AUTO_LENGTH, &modifier_val);
                napi_set_named_property(env, js_token, "modifier", modifier_val);
            }
            break;
        }
        default:
            break;
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
    Token *root = wiki_parse(wikitext, byte_length, cfg, false, 10);
    
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