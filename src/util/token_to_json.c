#include "util/token_to_json.h"

cJSON* token_to_json(const Token *token) {
    if (!token) return NULL;

    cJSON *root = cJSON_CreateObject();

    // 1. Basic Metadata
    cJSON_AddStringToObject(root, "type_name", token->type_name ? token->type_name : "unknown");
    if (token->name) cJSON_AddStringToObject(root, "name", token->name);

    // 2. Handle TokenData Union (Switch based on type)
    cJSON *data = cJSON_AddObjectToObject(root, "data");
	switch (token->type) {
		case TOKEN_HEADING:
			cJSON_AddNumberToObject(data, "level", token->data.heading.level);
			break;

		case TOKEN_COMMENT:
			cJSON_AddBoolToObject(data, "closed", token->data.comment.closed);
			break;

		case TOKEN_HTML:
			cJSON_AddBoolToObject(data, "self_closing", token->data.html.self_closing);
			cJSON_AddBoolToObject(data, "closing", token->data.html.closing);
			if (token->data.html.orig_tag) {
				cJSON_AddStringToObject(data, "orig_tag", token->data.html.orig_tag);
			}
			break;

		case TOKEN_TD:
			if (token->data.td.inner_syntax) {
				cJSON_AddStringToObject(data, "inner_syntax", token->data.td.inner_syntax);
			}
			break;

		case TOKEN_DOUBLE_UNDERSCORE:
			cJSON_AddBoolToObject(data, "case_sensitive", token->data.dunder.case_sensitive);
			cJSON_AddBoolToObject(data, "fullwidth", token->data.dunder.fullwidth);
			break;

		case TOKEN_QUOTE:
			cJSON_AddBoolToObject(data, "bold", token->data.quote.bold);
			cJSON_AddBoolToObject(data, "italic", token->data.quote.italic);
			break;

		case TOKEN_REDIRECT:
			if (token->data.redirect.pre) cJSON_AddStringToObject(data, "pre", token->data.redirect.pre);
			if (token->data.redirect.post) cJSON_AddStringToObject(data, "post", token->data.redirect.post);
			if (token->data.redirect.link) cJSON_AddStringToObject(data, "link", token->data.redirect.link);
			if (token->data.redirect.display) cJSON_AddStringToObject(data, "display", token->data.redirect.display);
			break;

		case TOKEN_EXT:
			if (token->data.ext.name) cJSON_AddStringToObject(data, "name", token->data.ext.name);
			if (token->data.ext.attr) cJSON_AddStringToObject(data, "attr", token->data.ext.attr);
			if (token->data.ext.inner) cJSON_AddStringToObject(data, "inner", token->data.ext.inner);
			if (token->data.ext.closing) cJSON_AddStringToObject(data, "closing", token->data.ext.closing);
			cJSON_AddBoolToObject(data, "self_closing", token->data.ext.self_closing);
			break;

		case TOKEN_INCLUDE:
			if (token->data.include.tag) cJSON_AddStringToObject(data, "tag", token->data.include.tag);
			if (token->data.include.attr) cJSON_AddStringToObject(data, "attr", token->data.include.attr);
			if (token->data.include.inner) cJSON_AddStringToObject(data, "inner", token->data.include.inner);
			if (token->data.include.closing) cJSON_AddStringToObject(data, "closing", token->data.include.closing);
			break;

		case TOKEN_EXT_ATTR:
			if (token->data.ext_attr.equal) cJSON_AddStringToObject(data, "equal", token->data.ext_attr.equal);
			cJSON_AddNumberToObject(data, "quote_open", (int)token->data.ext_attr.quote_open);
			cJSON_AddNumberToObject(data, "quote_close", (int)token->data.ext_attr.quote_close);
			break;

		case TOKEN_MAGIC_LINK:
			if (token->data.image_param.raw_syntax) cJSON_AddStringToObject(data, "raw_syntax", token->data.image_param.raw_syntax);
			break;

		case TOKEN_EXT_LINK:
			if (token->data.ext_link.space) cJSON_AddStringToObject(data, "space", token->data.ext_link.space);
			break;

		case TOKEN_TRANSCLUDE:
			if (token->data.transclude.modifier) cJSON_AddStringToObject(data, "modifier", token->data.transclude.modifier);
			break;

		default:
			break;
	}

	// 3. Parsing State
	cJSON *state = cJSON_AddObjectToObject(root, "state");
	cJSON_AddNumberToObject(state, "seen_epoch", token->seen_epoch);
	cJSON_AddNumberToObject(state, "stage", token->stage);
	cJSON_AddBoolToObject(state, "include", token->include);
	cJSON_AddBoolToObject(state, "built", token->built);

    // 4. Handle Children (The Recursive Part)
    if (token->child_count > 0) {
        cJSON *children_arr = cJSON_AddArrayToObject(root, "children");
        for (size_t i = 0; i < token->child_count; i++) {
            Child *child = &token->children[i];
            cJSON *child_obj = cJSON_CreateObject();

            if (child->is_text) {
                cJSON_AddStringToObject(child_obj, "type", "text");
                // Using text_len because u.text might not be NUL-terminated
                cJSON_AddStringToObject(child_obj, "data", child->text ? child->text : "");
            } else {
                // RECURSION: Add the child token as a nested object
                cJSON_AddItemToObject(child_obj, "token", token_to_json(child->token));
            }
            cJSON_AddItemToArray(children_arr, child_obj);
        }
    }

    return root;
}

