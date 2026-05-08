#include <node_api.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* extern_tokenizer headers */
#include "parse.h"
#include "token.h"
#include "config.h"

/**
 * Helper to convert a JS configuration object into a C ParserConfig struct.
 * It uses JSON.stringify in JS to get a JSON string, then config_load_string in C.
 */
static napi_status get_config_from_js(napi_env env, napi_value config_obj, ParserConfig **out_cfg) {
    napi_value global, json, stringify, result;
    napi_get_global(env, &global);
    napi_value key_json, key_stringify;
    napi_create_string_utf8(env, "JSON", NAPI_AUTO_LENGTH, &key_json);
    napi_get_property(env, global, key_json, &json);
    napi_create_string_utf8(env, "stringify", NAPI_AUTO_LENGTH, &key_stringify);
    napi_get_property(env, json, key_stringify, &stringify);

    // Call JSON.stringify(config_obj)
    napi_call_function(env, json, stringify, 1, &config_obj, &result);

    // Get the resulting JSON string
    size_t len;
    napi_get_value_string_utf8(env, result, NULL, 0, &len);
    char *json_str = malloc(len + 1);
    if (!json_str) return napi_generic_failure;
    napi_get_value_string_utf8(env, result, json_str, len + 1, &len);

    // Load the config using the existing C API
    *out_cfg = config_load_string(json_str, len);
    free(json_str);

    if (*out_cfg == NULL) {
        return napi_generic_failure;
    }
    return napi_ok;
}

/**
 * Recursively converts a Token tree into a JavaScript object.
 * This mirrors the structure of the JS Token class.
 */
static napi_value token_to_js(napi_env env, const Token *token) {
    napi_value js_token;
    napi_create_object(env, &js_token);

    // type
    napi_value type_val;
    napi_create_string_utf8(env, token->type_name, NAPI_AUTO_LENGTH, &type_val);
    // Correct function: napi_set_named_property
    napi_set_named_property(env, js_token, "type", type_val);

    // name (optional)
    if (token->name) {
        napi_value name_val;
        napi_create_string_utf8(env, token->name, NAPI_AUTO_LENGTH, &name_val);
        // Correct function: napi_set_named_property
        napi_set_named_property(env, js_token, "name", name_val);
    }

    // children
    if (token->child_count > 0) {
        napi_value children_array;
        napi_create_array(env, &children_array);
        for (size_t i = 0; i < token->child_count; i++) {
            Child *child = &token->children[i];
            if (child->is_text) {
                napi_value text_val;
                napi_create_string_utf8(env, child->text, child->text_len, &text_val);
                napi_set_element(env, children_array, i, text_val);
            } else {
                napi_value child_js = token_to_js(env, child->token);
                napi_set_element(env, children_array, i, child_js);
            }
        }
        napi_set_named_property(env, js_token, "children", children_array);
    }

    // data (union payload)
    napi_value data_obj;
    napi_create_object(env, &data_obj);
    bool has_data = false;

    switch (token->type) {
        case TOKEN_HEADING: {
            napi_value level_val;
            napi_create_int32(env, token->data.heading.level, &level_val);
            napi_set_named_property(env, data_obj, "level", level_val);
            has_data = true;
            break;
        }
        case TOKEN_COMMENT: {
            napi_value closed_val;
            napi_get_boolean(env, token->data.comment.closed, &closed_val);
            napi_set_named_property(env, data_obj, "closed", closed_val);
            has_data = true;
            break;
        }
        case TOKEN_HTML: {
            napi_value self_closing_val, closing_val, orig_tag_val;
            napi_get_boolean(env, token->data.html.self_closing, &self_closing_val);
            napi_get_boolean(env, token->data.html.closing, &closing_val);
            napi_set_named_property(env, data_obj, "self_closing", self_closing_val);
            napi_set_named_property(env, data_obj, "closing", closing_val);
            if (token->data.html.orig_tag) {
                napi_create_string_utf8(env, token->data.html.orig_tag, NAPI_AUTO_LENGTH, &orig_tag_val);
                napi_set_named_property(env, data_obj, "orig_tag", orig_tag_val);
            }
            has_data = true;
            break;
        }
        case TOKEN_TD: {
            if (token->data.td.inner_syntax) {
                napi_value inner_syntax_val;
                napi_create_string_utf8(env, token->data.td.inner_syntax, NAPI_AUTO_LENGTH, &inner_syntax_val);
                napi_set_named_property(env, data_obj, "inner_syntax", inner_syntax_val);
            }
            has_data = true;
            break;
        }
        case TOKEN_DOUBLE_UNDERSCORE: {
            napi_value case_sensitive_val, fullwidth_val;
            napi_get_boolean(env, token->data.dunder.case_sensitive, &case_sensitive_val);
            napi_get_boolean(env, token->data.dunder.fullwidth, &fullwidth_val);
            napi_set_named_property(env, data_obj, "case_sensitive", case_sensitive_val);
            napi_set_named_property(env, data_obj, "fullwidth", fullwidth_val);
            has_data = true;
            break;
        }
        case TOKEN_QUOTE: {
            napi_value bold_val, italic_val;
            napi_get_boolean(env, token->data.quote.bold, &bold_val);
            napi_get_boolean(env, token->data.quote.italic, &italic_val);
            napi_set_named_property(env, data_obj, "bold", bold_val);
            napi_set_named_property(env, data_obj, "italic", italic_val);
            has_data = true;
            break;
        }
        case TOKEN_REDIRECT: {
            if (token->data.redirect.pre) {
                napi_value pre_val;
                napi_create_string_utf8(env, token->data.redirect.pre, NAPI_AUTO_LENGTH, &pre_val);
                napi_set_named_property(env, data_obj, "pre", pre_val);
            }
            if (token->data.redirect.post) {
                napi_value post_val;
                napi_create_string_utf8(env, token->data.redirect.post, NAPI_AUTO_LENGTH, &post_val);
                napi_set_named_property(env, data_obj, "post", post_val);
            }
            if (token->data.redirect.link) {
                napi_value link_val;
                napi_create_string_utf8(env, token->data.redirect.link, NAPI_AUTO_LENGTH, &link_val);
                napi_set_named_property(env, data_obj, "link", link_val);
            }
            if (token->data.redirect.display) {
                napi_value display_val;
                napi_create_string_utf8(env, token->data.redirect.display, NAPI_AUTO_LENGTH, &display_val);
                napi_set_named_property(env, data_obj, "display", display_val);
            }
            has_data = true;
            break;
        }
        case TOKEN_EXT: {
            if (token->data.ext.name) {
                napi_value name_val;
                napi_create_string_utf8(env, token->data.ext.name, NAPI_AUTO_LENGTH, &name_val);
                napi_set_named_property(env, data_obj, "name", name_val);
            }
            if (token->data.ext.attr) {
                napi_value attr_val;
                napi_create_string_utf8(env, token->data.ext.attr, NAPI_AUTO_LENGTH, &attr_val);
                napi_set_named_property(env, data_obj, "attr", attr_val);
            }
            if (token->data.ext.inner) {
                napi_value inner_val;
                napi_create_string_utf8(env, token->data.ext.inner, NAPI_AUTO_LENGTH, &inner_val);
                napi_set_named_property(env, data_obj, "inner", inner_val);
            }
            if (token->data.ext.closing) {
                napi_value closing_val;
                napi_create_string_utf8(env, token->data.ext.closing, NAPI_AUTO_LENGTH, &closing_val);
                napi_set_named_property(env, data_obj, "closing", closing_val);
            }
            napi_value self_closing_val;
            napi_get_boolean(env, token->data.ext.self_closing, &self_closing_val);
            napi_set_named_property(env, data_obj, "self_closing", self_closing_val);
            has_data = true;
            break;
        }
        case TOKEN_INCLUDE: {
            if (token->data.include.tag) {
                napi_value tag_val;
                napi_create_string_utf8(env, token->data.include.tag, NAPI_AUTO_LENGTH, &tag_val);
                napi_set_named_property(env, data_obj, "tag", tag_val);
            }
            if (token->data.include.attr) {
                napi_value attr_val;
                napi_create_string_utf8(env, token->data.include.attr, NAPI_AUTO_LENGTH, &attr_val);
                napi_set_named_property(env, data_obj, "attr", attr_val);
            }
            if (token->data.include.inner) {
                napi_value inner_val;
                napi_create_string_utf8(env, token->data.include.inner, NAPI_AUTO_LENGTH, &inner_val);
                napi_set_named_property(env, data_obj, "inner", inner_val);
            }
            if (token->data.include.closing) {
                napi_value closing_val;
                napi_create_string_utf8(env, token->data.include.closing, NAPI_AUTO_LENGTH, &closing_val);
                napi_set_named_property(env, data_obj, "closing", closing_val);
            }
            has_data = true;
            break;
        }
        case TOKEN_EXT_ATTR: {
            if (token->data.ext_attr.equal) {
                napi_value equal_val;
                napi_create_string_utf8(env, token->data.ext_attr.equal, NAPI_AUTO_LENGTH, &equal_val);
                napi_set_named_property(env, data_obj, "equal", equal_val);
            }
            napi_value quote_open_val, quote_close_val;
            napi_get_boolean(env, token->data.ext_attr.quote_open != '\0', &quote_open_val);
            napi_get_boolean(env, token->data.ext_attr.quote_close != '\0', &quote_close_val);
            napi_set_named_property(env, data_obj, "quote_open", quote_open_val);
            napi_set_named_property(env, data_obj, "quote_close", quote_close_val);
            has_data = true;
            break;
        }
        case TOKEN_EXT_LINK: {
            if (token->data.ext_link.space) {
                napi_value space_val;
                napi_create_string_utf8(env, token->data.ext_link.space, NAPI_AUTO_LENGTH, &space_val);
                napi_set_named_property(env, data_obj, "space", space_val);
            }
            has_data = true;
            break;
        }
        case TOKEN_TRANSCLUDE: {
            if (token->data.transclude.modifier) {
                napi_value modifier_val;
                napi_create_string_utf8(env, token->data.transclude.modifier, NAPI_AUTO_LENGTH, &modifier_val);
                napi_set_named_property(env, data_obj, "modifier", modifier_val);
            }
            has_data = true;
            break;
        }
        default:
            break;
    }

    if (has_data) {
        napi_set_named_property(env, js_token, "data", data_obj);
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
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok || argc < 2) {
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

    // 4. Now get the underlying ArrayBuffer info
    void *buffer_data;
    size_t buffer_byte_length;
    napi_get_arraybuffer_info(env, buffer, &buffer_data, &buffer_byte_length);

    // The wikitext is a view into the buffer starting at the offset
    const char *wikitext = (const char *)((char *)buffer_data + byte_offset);

    // 2. Extract config from args[1]
    napi_value config_wrapper = args[1];
    napi_value config_obj;
    napi_value key_config;
    napi_create_string_utf8(env, "config", NAPI_AUTO_LENGTH, &key_config);
    napi_get_property(env, config_wrapper, key_config, &config_obj);

    ParserConfig *cfg = NULL;
    status = get_config_from_js(env, config_obj, &cfg);
    if (status != napi_ok || cfg == NULL) {
        napi_throw_error(env, NULL, "Failed to load config from JS object.");
        return NULL;
    }

    // 3. Call the C parser
    Token *root = wiki_parse(wikitext, cfg, false, 10);
    config_free(cfg);

    if (root == NULL) {
        napi_throw_error(env, NULL, "Wiki parse failed.");
        return NULL;
    }

    // 4. Convert the Token tree to a JS object
    napi_value js_root = token_to_js(env, root);

    // 5. Cleanup the C token tree
    token_free(root);

    return js_root;
}

/**
 * N-API Module Initialization
 */
napi_value Init(napi_env env, napi_value exports) {
    napi_property_descriptor desc = { "parse", 0, parse, 0, 0, 0, napi_default, 0 };
    napi_define_properties(env, exports, 1, &desc);
    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)