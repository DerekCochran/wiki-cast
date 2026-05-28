#include "util/token_to_json.h"
#include <stdlib.h>
#include <string.h>

static cJSON *token_text_to_json_string(const char *text, size_t text_len) {
    if (!text || text_len == 0) {
        return cJSON_CreateString("");
    }
    char *tmp = (char *)malloc(text_len + 1);
    if (!tmp) return cJSON_CreateString("");
    memcpy(tmp, text, text_len);
    tmp[text_len] = '\0';
    cJSON *s = cJSON_CreateString(tmp);
    free(tmp);
    return s ? s : cJSON_CreateString("");
}

cJSON* token_to_json(const Token *token) {
    if (!token) return NULL;

    cJSON *root = cJSON_CreateObject();

    // Basic metadata
    cJSON_AddStringToObject(root, "type", token->type_name ? token->type_name : "unknown");
    if (token->name) cJSON_AddStringToObject(root, "name", token->name);

	switch (token->type) {
		case TOKEN_HEADING:
			cJSON_AddNumberToObject(root, "level", token->data.heading.level);
			break;

		case TOKEN_COMMENT:
			cJSON_AddBoolToObject(root, "closed", token->data.comment.closed);
			break;

		case TOKEN_HTML:
			cJSON_AddBoolToObject(root, "selfClosing", token->data.html.self_closing);
			cJSON_AddBoolToObject(root, "closing", token->data.html.closing);
			if (token->data.html.orig_tag) {
				cJSON_AddStringToObject(root, "origTag", token->data.html.orig_tag);
			}
			break;

		case TOKEN_TD:
			if (token->data.td.inner_syntax) {
				cJSON_AddStringToObject(root, "innerSyntax", token->data.td.inner_syntax);
			}
			break;

		case TOKEN_DOUBLE_UNDERSCORE:
			cJSON_AddBoolToObject(root, "caseSensitive", token->data.dunder.case_sensitive);
			cJSON_AddBoolToObject(root, "fullwidth", token->data.dunder.fullwidth);
			break;

		case TOKEN_QUOTE:
			cJSON_AddBoolToObject(root, "bold", token->data.quote.bold);
			cJSON_AddBoolToObject(root, "italic", token->data.quote.italic);
			break;

		case TOKEN_REDIRECT:
			if (token->data.redirect.display) cJSON_AddStringToObject(root, "display", token->data.redirect.display);
			break;

		case TOKEN_EXT:
			if (token->data.ext.name) cJSON_AddStringToObject(root, "extName", token->data.ext.name);
			if (token->data.ext.attr) cJSON_AddStringToObject(root, "extAttr", token->data.ext.attr);
			if (token->data.ext.inner) cJSON_AddStringToObject(root, "extInner", token->data.ext.inner);
			if (token->data.ext.closing) cJSON_AddStringToObject(root, "extClosing", token->data.ext.closing);
			cJSON_AddBoolToObject(root, "extSelfClosing", token->data.ext.self_closing);
			break;

		case TOKEN_NOINCLUDE:
		case TOKEN_INCLUDE:
		case TOKEN_ONLYINCLUDE:
		case TOKEN_TRANSLATE:
			if (token->data.include.tag) cJSON_AddStringToObject(root, "tag", token->data.include.tag);
			if (token->data.include.attr) cJSON_AddStringToObject(root, "includeAttr", token->data.include.attr);
			if (token->data.include.inner) cJSON_AddStringToObject(root, "includeInner", token->data.include.inner);
			if (token->data.include.closing) cJSON_AddStringToObject(root, "includeClosing", token->data.include.closing);
			break;

		case TOKEN_EXT_ATTR:
			if (token->data.ext_attr.equal) cJSON_AddStringToObject(root, "equal", token->data.ext_attr.equal);
			{
				char qo[2] = {token->data.ext_attr.quote_open, '\0'};
				char qc[2] = {token->data.ext_attr.quote_close, '\0'};
				cJSON_AddStringToObject(root, "quoteOpen", qo);
				cJSON_AddStringToObject(root, "quoteClose", qc);
			}
			break;

		case TOKEN_PARAMETER:
			if (token->data.image_param.raw_syntax) cJSON_AddStringToObject(root, "rawSyntax", token->data.image_param.raw_syntax);
			break;

		case TOKEN_MAGIC_LINK:
		case TOKEN_EXT_LINK:
			if (token->data.ext_link.space) cJSON_AddStringToObject(root, "space", token->data.ext_link.space);
			break;

		case TOKEN_LINK:
		case TOKEN_FILE:
		case TOKEN_CATEGORY:
			cJSON_AddBoolToObject(root, "magicPipe", token->data.link.magic_pipe);
			break;

		case TOKEN_TRANSCLUDE:
		case TOKEN_ARG:
			if (token->data.transclude.modifier) cJSON_AddStringToObject(root, "modifier", token->data.transclude.modifier);
			break;

		default:
			break;
	}

    // Children
    if (token->child_count > 0) {
        cJSON *children_arr = cJSON_AddArrayToObject(root, "childNodes");
        for (size_t i = 0; i < token->child_count; i++) {
            Child *child = &token->children[i];
            if (child->is_text) {
                cJSON_AddItemToArray(children_arr, token_text_to_json_string(child->text, child->text_len));
            } else {
                cJSON_AddItemToArray(children_arr, token_to_json(child->token));
            }
        }
    }

    return root;
}

