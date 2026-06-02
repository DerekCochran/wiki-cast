#pragma once
#include "util/string_util.h"
#include "wiki_cast/config.h"
#include "accum.h"

/** Parse language-variant converter fragments `-{...}-` (stage 10).
 *  Operates on the working string `ws` and pushes created ConverterTokens
 *  into `accum` replacing the original fragments with sentinel markers.
 */
void parse_converter(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum);
