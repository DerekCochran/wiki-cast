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

static void json_add_cstr_to_object(cJSON *root, const char *key, const char *value) {
	if (!root || !key || !value) return;
	cJSON_AddStringToObject(root, key, value);
}

static void json_add_sz_view_to_object(cJSON *root, const char *key, sz_string_view_t view) {
	if (!root || !key || !view.start) return;
	cJSON_AddItemToObject(root, key, token_text_to_json_string(view.start, view.length));
}

static void json_add_char_to_object(cJSON *root, const char *key, char ch) {
	if (!root || !key) return;
	char buf[2] = { ch, '\0' };
	cJSON_AddStringToObject(root, key, buf);
}

cJSON* token_to_json(const Token *token) {
    if (!token) return NULL;

    cJSON *root = cJSON_CreateObject();
	sz_string_view_t subtype_name = token_subtype_name(token->subtype);

	// Core metadata/state for internal debugging.
	cJSON_AddItemToObject(root, "type", token_text_to_json_string(subtype_name.start ? subtype_name.start : "", subtype_name.length));
	// Token type is an enum, but we want to add the string here
	// cJSON_AddNumberToObject(root, "tokenType", token->type);
	cJSON_AddStringToObject(root, "Type", get_token_type_name(token->type));
	cJSON_AddStringToObject(root, "SubType", get_token_subtype_name(token->subtype));
	// cJSON_AddNumberToObject(root, "tokenSubtype", token->subtype);
	if (token->name) json_add_cstr_to_object(root, "name", token->name);
	cJSON_AddNumberToObject(root, "childCount", token->child_count);
	//cJSON_AddNumberToObject(root, "childCap", token->child_cap);
	//cJSON_AddNumberToObject(root, "stage", token->stage);
	if( token->include ) {
		cJSON_AddBoolToObject(root, "include", token->include);
	}
	//cJSON_AddBoolToObject(root, "built", token->built);
	if( token->ext_inner_context ) {
		cJSON_AddBoolToObject(root, "extInnerContext", token->ext_inner_context);
	}
	//cJSON_AddNumberToObject(root, "sepCode", (unsigned char)token->sep);
	//if (token->sep != '\0') {
	//	json_add_char_to_object(root, "sep", token->sep);
	//}

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
			json_add_sz_view_to_object(root, "origTag", token->data.html.orig_tag);
			cJSON_AddNumberToObject(root, "origTagLen", token->data.html.orig_tag.length);
			break;

		case TOKEN_TD:
			json_add_sz_view_to_object(root, "innerSyntax", token->data.td.inner_syntax);
			cJSON_AddNumberToObject(root, "innerSyntaxLen", token->data.td.inner_syntax.length);
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
			json_add_sz_view_to_object(root, "pre", token->data.redirect.pre);
			cJSON_AddNumberToObject(root, "preLen", token->data.redirect.pre.length);
			json_add_sz_view_to_object(root, "post", token->data.redirect.post);
			cJSON_AddNumberToObject(root, "postLen", token->data.redirect.post.length);
			json_add_sz_view_to_object(root, "link", token->data.redirect.link);
			cJSON_AddNumberToObject(root, "linkLen", token->data.redirect.link.length);
			json_add_sz_view_to_object(root, "display", token->data.redirect.display);
			cJSON_AddNumberToObject(root, "displayLen", token->data.redirect.display.length);
			break;

		case TOKEN_EXT:
			json_add_sz_view_to_object(root, "extName", token->data.ext.name);
			cJSON_AddNumberToObject(root, "extNameLen", token->data.ext.name.length);
			json_add_sz_view_to_object(root, "extAttr", token->data.ext.attr);
			cJSON_AddNumberToObject(root, "extAttrLen", token->data.ext.attr.length);
			json_add_sz_view_to_object(root, "extInner", token->data.ext.inner);
			cJSON_AddNumberToObject(root, "extInnerLen", token->data.ext.inner.length);
			json_add_sz_view_to_object(root, "extClosing", token->data.ext.closing);
			cJSON_AddNumberToObject(root, "extClosingLen", token->data.ext.closing.length);
			cJSON_AddBoolToObject(root, "extSelfClosing", token->data.ext.self_closing);
			break;

		case TOKEN_NOINCLUDE:
		case TOKEN_INCLUDE:
		case TOKEN_ONLYINCLUDE:
		case TOKEN_TRANSLATE:
			json_add_sz_view_to_object(root, "tag", token->data.include.tag);
			cJSON_AddNumberToObject(root, "tagLen", token->data.include.tag.length);
			json_add_sz_view_to_object(root, "includeAttr", token->data.include.attr);
			cJSON_AddNumberToObject(root, "includeAttrLen", token->data.include.attr.length);
			json_add_sz_view_to_object(root, "includeInner", token->data.include.inner);
			cJSON_AddNumberToObject(root, "includeInnerLen", token->data.include.inner.length);
			json_add_sz_view_to_object(root, "includeClosing", token->data.include.closing);
			cJSON_AddNumberToObject(root, "includeClosingLen", token->data.include.closing.length);
			break;

		case TOKEN_EXT_ATTR:
			json_add_sz_view_to_object(root, "equal", token->data.ext_attr.equal);
			cJSON_AddNumberToObject(root, "equalLen", token->data.ext_attr.equal.length);
			{
				cJSON_AddNumberToObject(root, "quoteOpenCode", (unsigned char)token->data.ext_attr.quote_open);
				cJSON_AddNumberToObject(root, "quoteCloseCode", (unsigned char)token->data.ext_attr.quote_close);
				if (token->data.ext_attr.quote_open != '\0') {
					json_add_char_to_object(root, "quoteOpen", token->data.ext_attr.quote_open);
				}
				if (token->data.ext_attr.quote_close != '\0') {
					json_add_char_to_object(root, "quoteClose", token->data.ext_attr.quote_close);
				}
			}
			break;

		case TOKEN_PLAIN:
			if (token->subtype == TOKEN_SUBTYPE_IMAGE_PARAMETER) {
				json_add_sz_view_to_object(root, "rawSyntax", token->data.image_param.raw_syntax);
				cJSON_AddNumberToObject(root, "rawSyntaxLen", token->data.image_param.raw_syntax.length);
			}
			break;

		case TOKEN_MAGIC_LINK:
		case TOKEN_EXT_LINK:
			json_add_sz_view_to_object(root, "space", token->data.ext_link.space);
			cJSON_AddNumberToObject(root, "spaceLen", token->data.ext_link.space.length);
			break;

		case TOKEN_LINK:
		case TOKEN_FILE:
		case TOKEN_CATEGORY:
			cJSON_AddBoolToObject(root, "magicPipe", token->data.link.magic_pipe);
			break;

		case TOKEN_TRANSCLUDE:
		case TOKEN_ARG:
			json_add_sz_view_to_object(root, "modifier", token->data.transclude.modifier);
			cJSON_AddNumberToObject(root, "modifierLen", token->data.transclude.modifier.length);
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
				cJSON *child_obj = cJSON_CreateObject();
				cJSON_AddStringToObject(child_obj, "kind", "text");
				cJSON_AddItemToObject(child_obj, "text", token_text_to_json_string(child->text, child->text_len));
				cJSON_AddNumberToObject(child_obj, "textLen", child->text_len);
				cJSON_AddBoolToObject(child_obj, "textOwned", child->text_owned);
				cJSON_AddItemToArray(children_arr, child_obj);
            } else {
				cJSON *child_obj = cJSON_CreateObject();
				// cJSON_AddStringToObject(child_obj, "kind", "token");
				// cJSON_AddItemToObject(child_obj, "token", token_to_json(child->token));
				// cJSON_AddItemToArray(children_arr, child_obj);
				cJSON_AddItemToArray(children_arr, token_to_json(child->token));
            }
        }
    }

    return root;
}

