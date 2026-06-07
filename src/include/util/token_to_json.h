#pragma once
#include <cjson/cJSON.h>
#include "wiki_cast/token.h"

cJSON* token_to_json(const Token *token);
