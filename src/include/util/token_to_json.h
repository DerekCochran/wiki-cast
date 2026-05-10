#pragma once
#include <cjson/cJSON.h>
#include "token.h"

cJSON* token_to_json(const Token *token);
