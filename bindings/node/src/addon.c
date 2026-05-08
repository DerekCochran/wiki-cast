#include <node_api.h>
#include <stdlib.h>
#include <stdio.h>

/* extern_tokenizer headers */
#include "parse.h"
#include "token.h"
#include "config.h"

static void external_buffer_cleanup(napi_env env, void *data, void *hint) { free(data); }

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
