#include <node_api.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

/* extern_tokenizer headers */
#include "parse.h"
#include "token.h"
#include "config.h"

/* Callback closure holding a copy of the serialized text string */
typedef struct {
  char *text;   /* owned copy of the reconstructed wikitext */
} tostr_closure;

static void tostr_finalize(napi_env env, void* finalize_data, void* finalize_hint) {
  tostr_closure *c = (tostr_closure *)finalize_data;
  if (!c) return;
  free(c->text);
  free(c);
}

static napi_value tostr_callback(napi_env env, napi_callback_info info) {
  napi_status status;
  void *data = NULL;
  status = napi_get_cb_info(env, info, NULL, NULL, NULL, &data);
  if (status != napi_ok || !data) return NULL;
  tostr_closure *c = (tostr_closure *)data;
  napi_value text_val;
  napi_status rs = napi_create_string_utf8(env, c->text ? c->text : "", NAPI_AUTO_LENGTH, &text_val);
  (void)rs;
  return text_val;
}

static bool get_token_config_json(napi_env env, napi_value token, char **json_out, size_t *json_len_out) {
  napi_value get_attr_fn;
  napi_status status = napi_get_named_property(env, token, "getAttribute", &get_attr_fn);
  if (status != napi_ok) return false;

  napi_value key;
  status = napi_create_string_utf8(env, "config", NAPI_AUTO_LENGTH, &key);
  if (status != napi_ok) return false;

  napi_value argv[1] = { key };
  napi_value config_val;
  status = napi_call_function(env, token, get_attr_fn, 1, argv, &config_val);
  if (status != napi_ok) return false;

  napi_value global;
  napi_value json_obj;
  napi_value stringify_fn;
  napi_value json_str;
  status = napi_get_global(env, &global);
  if (status != napi_ok) return false;
  status = napi_get_named_property(env, global, "JSON", &json_obj);
  if (status != napi_ok) return false;
  status = napi_get_named_property(env, json_obj, "stringify", &stringify_fn);
  if (status != napi_ok) return false;

  napi_value stringify_argv[1] = { config_val };
  status = napi_call_function(env, json_obj, stringify_fn, 1, stringify_argv, &json_str);
  if (status != napi_ok) return false;

  size_t json_len = 0;
  status = napi_get_value_string_utf8(env, json_str, NULL, 0, &json_len);
  if (status != napi_ok) return false;

  char *json_buf = malloc(json_len + 1);
  assert(json_buf);
  status = napi_get_value_string_utf8(env, json_str, json_buf, json_len + 1, &json_len);
  if (status != napi_ok) {
    free(json_buf);
    return false;
  }

  *json_out = json_buf;
  if (json_len_out) *json_len_out = json_len;
  return true;
}

/* Recursively concatenate token tree text (matches JS token.toString()).
 * This is replaced by token_to_string() in the shared library and uses a
 * leased thread-local scratch buffer to avoid a separate heap allocation. */

static napi_value parse_wrapped(napi_env env, napi_callback_info info) {
  napi_status status;
  size_t argc = 16;
  napi_value argv[16];
  napi_value this_arg;
  status = napi_get_cb_info(env, info, &argc, argv, &this_arg, NULL);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to get callback info");
    return NULL;
  }

  /* Obtain the original wikitext from `this.firstChild.toString()` if present,
   * otherwise fall back to this.toString(). */
  napi_value firstChild;
  napi_value js_wikitext_val = NULL;
  if (napi_get_named_property(env, this_arg, "firstChild", &firstChild) == napi_ok && firstChild != NULL) {
    napi_value tostr_fn;
    if (napi_get_named_property(env, firstChild, "toString", &tostr_fn) == napi_ok && tostr_fn != NULL) {
      status = napi_call_function(env, firstChild, tostr_fn, 0, NULL, &js_wikitext_val);
    }
  }
  if (js_wikitext_val == NULL) {
    napi_value tostr_fn;
    if (napi_get_named_property(env, this_arg, "toString", &tostr_fn) == napi_ok && tostr_fn != NULL) {
      status = napi_call_function(env, this_arg, tostr_fn, 0, NULL, &js_wikitext_val);
    }
  }
  if (js_wikitext_val == NULL) {
    napi_throw_error(env, NULL, "Unable to obtain wikitext string from Token instance");
    return NULL;
  }

  /* Convert JS string to UTF-8 C string */
  size_t wlen = 0;
  status = napi_get_value_string_utf8(env, js_wikitext_val, NULL, 0, &wlen);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to measure wikitext length");
    return NULL;
  }
  char *wtext = malloc(wlen + 1);
  assert(wtext);
  status = napi_get_value_string_utf8(env, js_wikitext_val, wtext, wlen + 1, &wlen);
  if (status != napi_ok) {
    free(wtext);
    napi_throw_error(env, NULL, "Failed to copy wikitext string");
    return NULL;
  }

  /* Parse args: argv[0] => max_stage (number), argv[1] => include (boolean) */
  int max_stage = 11; /* default used by tests */
  bool include = false;
  if (argc >= 1) {
    napi_valuetype vt;
    if (napi_typeof(env, argv[0], &vt) == napi_ok && vt == napi_number) {
      int64_t v = 0;
      napi_get_value_int64(env, argv[0], &v);
      max_stage = (int)v;
    }
  }
  if (argc >= 2) {
    napi_valuetype vt;
    if (napi_typeof(env, argv[1], &vt) == napi_ok && vt == napi_boolean) {
      bool b = false;
      napi_get_value_bool(env, argv[1], &b);
      include = b;
    }
  }

  /* Load parser config from the calling JS Token instance. */
  char *cfg_json = NULL;
  size_t cfg_json_len = 0;
  if (!get_token_config_json(env, this_arg, &cfg_json, &cfg_json_len)) {
    free(wtext);
    napi_throw_error(env, NULL, "Failed to read parser config from Token instance");
    return NULL;
  }

  /* If requested, emit the raw config JSON, args, and wikitext to a log dir. */
  const char *stage_log_dir = getenv("WIKI_STAGE_LOG_DIR");
  if (stage_log_dir && stage_log_dir[0] != '\0') {
    /* Best-effort: create the directory */
    if (mkdir(stage_log_dir, 0777) != 0 && errno != EEXIST) {
      /* ignore mkdir failures */
    }

    char runid[64];
    unsigned long uniq = (unsigned long)((uintptr_t)wtext & 0xfffffffful);
    snprintf(runid, sizeof(runid), "%d-%ld-%lu", (int)getpid(), (long)time(NULL), uniq);

    /* Append a compact metadata block to native-stage.log for convenience. */
    char pathbuf[1024];
    snprintf(pathbuf, sizeof(pathbuf), "%s/native-stage.log", stage_log_dir);
    FILE *nf = fopen(pathbuf, "a");
    if (nf) {
      fprintf(nf, "--- Native Metadata %s --\n", runid);
      fprintf(nf, "max_stage=%d\n", max_stage);
      fprintf(nf, "include=%d\n", include ? 1 : 0);
      fprintf(nf, "config:\n%s\n", cfg_json);
      fprintf(nf, "wikitext:\n");
      fwrite(wtext, 1, wlen, nf);
      fprintf(nf, "\n\n");
      fclose(nf);
    }
  }

  ParserConfig *cfg = config_load_string(cfg_json, cfg_json_len);
  free(cfg_json);
  if (!cfg) {
    free(wtext);
    napi_throw_error(env, NULL, "Failed to load parser config from Token instance");
    return NULL;
  }

  /* Call the C parser */
  Token *root = wiki_parse(wtext, cfg, include, max_stage);
  if (!root) {
    config_free(cfg);
    free(wtext);
    napi_throw_error(env, NULL, "C parser returned NULL");
    return NULL;
  }

  /* Serialize token tree to JSON in-memory */
  char *json_buf = NULL;
  size_t json_len = 0;
  FILE *jf = open_memstream(&json_buf, &json_len);
  if (!jf) {
    token_free(root);
    config_free(cfg);
    free(wtext);
    napi_throw_error(env, NULL, "open_memstream failed");
    return NULL;
  }
  token_to_json(root, jf);
  fclose(jf);

  /* Build the reconstructed string from token tree */
  ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();
  char *text_buf = token_to_string(root, scratch);

  /* Parse JSON into a JS object: JSON.parse(json_buf) */
  napi_value global, json_obj, parse_fn, js_json_str, root_obj;
  status = napi_get_global(env, &global);
  status = napi_get_named_property(env, global, "JSON", &json_obj);
  status = napi_get_named_property(env, json_obj, "parse", &parse_fn);
  status = napi_create_string_utf8(env, json_buf, (size_t)json_len, &js_json_str);
  napi_value parse_args[1] = { js_json_str };
  status = napi_call_function(env, json_obj, parse_fn, 1, parse_args, &root_obj);

  if (status != napi_ok) {
    token_free(root);
    config_free(cfg);
    free(wtext);
    free(json_buf);
    if (status == napi_pending_exception) {
      /* JSON.parse threw — re-throw the pending exception as-is */
      return NULL;
    }
    napi_throw_error(env, NULL, "JSON.parse call failed");
    return NULL;
  }

  /* Verify root_obj is actually an object before setting properties */
  napi_valuetype root_vt = napi_undefined;
  napi_typeof(env, root_obj, &root_vt);
  if (root_vt != napi_object) {
    token_free(root);
    config_free(cfg);
    free(wtext);
    free(json_buf);
    napi_throw_error(env, NULL, "JSON.parse did not return an object");
    return NULL;
  }

  /* Create a toString() function backed by a C string copy.
   * Storing text as plain char* avoids the napi_create_reference restriction
   * on primitive values in Node.js >=12.  The closure is attached to
   * root_obj (a real JS object) via napi_wrap so it's freed on GC. */
  tostr_closure *closure = malloc(sizeof(tostr_closure));
  assert(closure);
  closure->text = strdup(text_buf);
  assert(closure->text);
  wiki_thread_buf_release_scratch(scratch);

  napi_value tostr_fn;
  status = napi_create_function(env, "toString", NAPI_AUTO_LENGTH, tostr_callback, closure, &tostr_fn);
  if (status != napi_ok) {
    tostr_finalize(NULL, closure, NULL);
    token_free(root);
    config_free(cfg);
    free(wtext);
    free(json_buf);
    napi_throw_error(env, NULL, "Failed to create toString function");
    return NULL;
  }

  /* Attach closure lifetime to root_obj (a plain JS object — safe for napi_wrap). */
  napi_wrap(env, root_obj, closure, tostr_finalize, NULL, NULL);

  /* Set the toString property on the parsed root object */
  status = napi_set_named_property(env, root_obj, "toString", tostr_fn);

  /* Cleanup C-side (we already copied necessary JS strings) */
  token_free(root);
  config_free(cfg);
  free(wtext);
  free(json_buf);

  return root_obj;
}

static napi_value Init(napi_env env, napi_value exports) {
  napi_status status;
  napi_value parse_fn;
  status = napi_create_function(env, "parse", NAPI_AUTO_LENGTH, parse_wrapped, NULL, &parse_fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to create parse function");
    return NULL;
  }
  status = napi_set_named_property(env, exports, "parse", parse_fn);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to export parse function");
    return NULL;
  }
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
