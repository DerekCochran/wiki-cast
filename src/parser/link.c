/* link.c — Link token constructors (LinkToken, FileToken, CategoryToken)
 *
 * Mirrors JS LinkBaseToken architecture:
 * - LinkToken for internal wiki links [[ ... ]]
 * - FileToken for file/image links [[ File: ... ]]
 * - CategoryToken for category links [[ Category: ... ]]
 *
 * All three share the same constructor function with type dispatch.
 */

#include "parser/link.h"
#include "accum.h"
#include "parser/quotes.h"
#include "token.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static bool is_magic_pipe_delimiter(const char *delimiter) {
	if(!delimiter) return false;
	if((unsigned char)delimiter[0] != 0x00) return false;
	size_t i = 1;
	if(!(delimiter[i] >= '0' && delimiter[i] <= '9')) return false;
	while(delimiter[i] >= '0' && delimiter[i] <= '9') i++;
	return delimiter[i] == '!' && (unsigned char)delimiter[i + 1] == 0x7F;
}

/**
 * Create a LinkToken, FileToken, or CategoryToken.
 *
 * Mirrors JS LinkBaseToken constructor — basic token factory.
 * - Creates the main token with given type and type_name
 * - Creates a link-target atom child containing the raw link string
 * - Pushes main token into accum
 *
 * The caller is responsible for:
 * - Parsing the text parameter via parse_inner_fragment or append_file_image_params
 * - Appending the parsed text token as a child via token_append_child
 *
 * Parameters:
 *   type: TOKEN_LINK, TOKEN_FILE, or TOKEN_CATEGORY
 *   type_name: "link", "file", or "category"
 *   link: raw link target string (may contain sentinels)
 *   link_len: byte length of link
 *   text: raw text or NULL (for informational purposes; not used by this function)
 *   text_len: byte length of text (unused)
 *   delimiter: non-NULL if a delimiter exists; used to preserve {{!}} delimiter parity
 *   cfg, accum: required; tidy: unused
 *
 * Returns the created token (already pushed to accum).
 */
Token *create_link_token(TokenType type, const char *type_name,
												 const char *link, size_t link_len,
												 const char *text, size_t text_len,
												 const char *delimiter,
												 const ParserConfig *cfg,
												 Accum *accum,
												 bool tidy) {
	(void)text;
	(void)text_len;
	(void)cfg;
	(void)tidy;

	if(!accum) return NULL;

	/* Create the main link token and push to accum. */
	Token *tok= token_new(type, type_name);
	if(!tok) return NULL;
	tok->data.link.magic_pipe = is_magic_pipe_delimiter(delimiter);
	accum_push(accum, tok);

	/* Child 0: link-target atom containing the raw link string. */
	Token *target= token_new(TOKEN_ATOM, "link-target");
	if(target) {
		if(link && link_len > 0) {
			/* Persist the link text into the tokens arena to avoid dangling views */
			const char *link_view = wiki_thread_buf_append_to_tokens(link, link_len);
			token_append_text_n(target, link_view, link_len);
		}
		accum_push(accum, target);
		token_append_child(tok, target);
	}

	return tok;
}
