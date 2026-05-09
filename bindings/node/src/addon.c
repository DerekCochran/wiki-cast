#include <node_api.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* extern_tokenizer headers */
#include "parse.h"
#include "token.h"
#include "config.h"

/** Prorotypes for the tokens */
static napi_value token_prototype = NULL;

static napi_status token_finalizer(napi_env env, void *finalize_data, void *finalize_context) {
    Token *token = (Token *)finalize_data;
    token_free(token);
    return napi_ok;
}

static napi_status toString_wrapper(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok) return status;

    Token *token;
    status = napi_unwrap(env, args[0], (void **)&token);
    if (status != napi_ok) return status;

    ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();
    char *str = token_to_string(token, scratch);
    
    if (!str) {
        wiki_thread_buf_release_scratch(scratch);
        return napi_generic_failure;
    }

    napi_value result;
    status = napi_create_string_utf8(env, str, scratch->len, &result);
    
    wiki_thread_buf_release_scratch(scratch);
    return status;
}

static napi_status json_stringify_wrapper(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_status status = napi_get_cb_info(env, info, &argc, args, NULL, NULL);
    if (status != napi_ok) return status;

    Token *token;
    status = napi_unwrap(env, args[0], (void **)&token);
    if (status != napi_ok) return status;

    ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();
    
    json_stringify_wikiparser_node(token, scratch);
    
    napi_value result;
    status = napi_create_string_utf8(env, scratch->buf, scratch->len, &result);
    
    wiki_thread_buf_release_scratch(scratch);
    return status;
}

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
static napi_value token_to_js(napi_env env, const Token *token, bool wrap_root) {
    napi_value js_token;
    napi_create_object(env, &js_token);

    if (wrap_root && token_prototype != NULL) {
        napi_set_prototype(env, js_token, token_prototype);
    }

    if (wrap_root) {
        napi_wrap(env, js_token, (void *)token, token_finalizer, NULL, NULL);
    }

    // type
    napi_value type_val;
    napi_create_string_utf8(env, token->type_name, NAPI_AUTO_LENGTH, &type_val);
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
                napi_value child_js = token_to_js(env, child->token, false);
                napi_set_element(env, children_array, i, child_js);
            }
        }
        napi_set_named_property(env, js_token, "childNodes", children_array);
    }

    // data (union payload)
    napi_value data_obj;
    napi_create_object(env, &data_obj);
    bool has_data = false;


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
    napi_value js_root = token_to_js(env, root, true);

    // 5. Cleanup the C token tree
    token_free(root);

    return js_root;
}

/**
 * N-API Module Initialization
 */
napi_value Init(napi_env env, napi_value exports) {
    // 2. Create the parse function
    napi_property_descriptor desc = { "parse", 0, parse, 0, 0, 0, napi_default, 0 };
    napi_define_properties(env, exports, 1, &desc);

    // 2. Create a prototype for the tokens
    napi_value proto;
    napi_create_object(env, &proto);
    napi_property_descriptor proto_descs[] = {
        { "toString", 0, toString_wrapper, 0, 0, 0, napi_default, 0 },
        { "jsonStringifyWikiparserNode", 0, json_stringify_wrapper, 0, 0, 0, napi_default, 0 }
    };
    napi_define_properties(env, proto, 2, proto_descs);
    token_prototype = proto;

    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)