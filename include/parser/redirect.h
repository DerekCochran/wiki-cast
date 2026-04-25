/*
 * redirect.h — Stage 0a: redirect detection (parseRedirect).
 *
 * Mirrors dist/parser/redirect.js.
 */
#pragma once
#include "../token.h"
#include "../accum.h"
#include "../config.h"
#include "../string_util.h"
#include <stdbool.h>

/**
 * Try to parse a redirect from the working string.
 *
 * If the wikitext starts with a redirect keyword followed by [[target]],
 * a RedirectToken is pushed to `accum`, the working string is updated to
 * "\0<N>o\x7F<remainder>", and true is returned.
 *
 * If no redirect is found nothing is modified and false is returned.
 *
 * @param ws     Working string (in/out).
 * @param cfg    Parser config.
 * @param accum  Accumulator (redirect token is pushed here).
 * @return       true if a redirect was detected.
 */
bool parse_redirect(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum);
