#include <node_api.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* extern_tokenizer headers */
#include "parse.h"
#include "token.h"
#include "config.h"

static napi_ref token_prototype_ref; // Use a reference to keep it alive

static void token_finalizer(napi_env env, void *finalize_data, void *finalize_context) {
    Token *token = (Token *)finalize_data;
    token_free(token);
    return;
}

static napi_value toString_wrapper(napi_env env, napi_callback_info info) {
    size_t argc = 1;
    napi_value args[1];
    napi_value this_arg;
    // We usually unwrap 'this' if these are methods on the prototype
    napi_status status = napi_get_cb_info(env, info, &argc, args, &this_arg, NULL);
    
    // If no argument was passed, we might be calling this as a method: obj.toString()
    napi_value target = (argc > 0) ? args[0] : this_arg;

    Token *token;
    status = napi_unwrap(env, target, (void **)&token);
    if (status != napi_ok) {
        napi_throw_error(env, NULL, "Failed to unwrap Token object");
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
    napi_get_value_string_utf8(env, config_val, path, path_len + 1, &path_len);

    ParserConfig* cfg = config_load_file(path);
    free(path); 
    
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
        napi_value proto;
        napi_get_reference_value(env, token_prototype_ref, &proto);
        napi_set_named_property(env, js_token, "__proto__", proto);
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

    // 3. Call the C parser
    Token *root = wiki_parse(wikitext, byte_length, cfg, false, 10);
    config_free(cfg);

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
    napi_property_descriptor desc = { "parse", 0, parse, 0, 0, 0, napi_default, 0 };
    napi_define_properties(env, exports, 1, &desc);

    napi_value proto;
    napi_create_object(env, &proto);
    
    napi_property_descriptor proto_descs[] = {
        { "toString", 0, toString_wrapper, 0, 0, 0, napi_default, 0 }
    };
    napi_define_properties(env, proto, 2, proto_descs);

    // Create a persistent reference so the prototype lives forever
    napi_create_reference(env, proto, 1, &token_prototype_ref);

    return exports;
}
NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)