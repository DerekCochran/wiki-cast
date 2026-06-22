#!/usr/bin/env python3
"""
WikiCast to Markdown converter.

This sample needs work!!!!!
"""

import os
import sys
from urllib.parse import quote

from wiki_cast import WikiConfig, WikiParser, TokenSubType, TokenType


WIKI_ROOT_URL = "https://en.wikipedia.org/wiki/"


class MarkdownConverter:
    def __init__(self):
        self.output = []
        self.list_level = 0
        self.table_rows = []
        self.current_row = []
        self.footnotes = []
        self.footnote_index_by_name = {}
        self.references_emitted = False
        self.type_handlers = {
            TokenType.TOKEN_TEXT: self._h_text,
            TokenType.TOKEN_ROOT: self._hs_root,
            TokenType.TOKEN_PLAIN: self._h_plain,
            TokenType.TOKEN_COMMENT: self._h_comment,
            TokenType.TOKEN_EXT: self._h_ext,
            TokenType.TOKEN_NOINCLUDE: self._h_include_like,
            TokenType.TOKEN_INCLUDE: self._h_include_like,
            TokenType.TOKEN_TRANSLATE: self._h_translate,
            TokenType.TOKEN_ONLYINCLUDE: self._h_include_like,
            TokenType.TOKEN_ARG: self._h_arg,
            TokenType.TOKEN_TRANSCLUDE: self._h_transclude,
            TokenType.TOKEN_HEADING: self._h_heading,
            TokenType.TOKEN_HTML: self._h_html,
            TokenType.TOKEN_TABLE: self._h_table,
            TokenType.TOKEN_TR: self._h_tr,
            TokenType.TOKEN_TD: self._h_td,
            TokenType.TOKEN_HR: self._h_hr,
            TokenType.TOKEN_DOUBLE_UNDERSCORE: self._h_double_underscore,
            TokenType.TOKEN_LINK: self._h_link,
            TokenType.TOKEN_FILE: self._h_file,
            TokenType.TOKEN_CATEGORY: self._h_category,
            TokenType.TOKEN_REDIRECT: self._h_redirect,
            TokenType.TOKEN_REDIRECT_TARGET: self._h_passthrough,
            TokenType.TOKEN_REDIRECT_SYNTAX: self._h_skip,
            TokenType.TOKEN_QUOTE: self._h_quote,
            TokenType.TOKEN_EXT_LINK: self._h_ext_link,
            TokenType.TOKEN_MAGIC_LINK: self._h_magic_link,
            TokenType.TOKEN_LIST: self._h_list,
            TokenType.TOKEN_DD: self._h_dd,
            TokenType.TOKEN_CONVERTER: self._h_passthrough,
            TokenType.TOKEN_PARAMETER: self._h_parameter,
            TokenType.TOKEN_ATTRIBUTES: self._h_passthrough,
            TokenType.TOKEN_SYNTAX: self._h_skip,
            TokenType.TOKEN_ATOM: self._h_passthrough,
            TokenType.TOKEN_HIDDEN: self._h_skip,
            TokenType.TOKEN_EXT_ATTRS: self._h_passthrough,
            TokenType.TOKEN_EXT_INNER: self._h_passthrough,
            TokenType.TOKEN_EXT_ATTR_DIRTY: self._h_passthrough,
            TokenType.TOKEN_EXT_ATTR: self._h_passthrough,
            TokenType.TOKEN_ATTR_KEY: self._h_passthrough,
            TokenType.TOKEN_ATTR_VALUE: self._h_passthrough,
        }
        self.subtype_handlers = {
            TokenSubType.TOKEN_SUBTYPE_NONE: self._hs_none,
            TokenSubType.TOKEN_SUBTYPE_ROOT: self._hs_root,
            TokenSubType.TOKEN_SUBTYPE_REDIRECT: self._hs_redirect,
            TokenSubType.TOKEN_SUBTYPE_REDIRECT_SYNTAX: self._hs_redirect_syntax,
            TokenSubType.TOKEN_SUBTYPE_REDIRECT_TARGET: self._hs_redirect_target,
            TokenSubType.TOKEN_SUBTYPE_COMMENT: self._hs_comment,
            TokenSubType.TOKEN_SUBTYPE_EXT: self._hs_ext,
            TokenSubType.TOKEN_SUBTYPE_NOINCLUDE: self._hs_noinclude,
            TokenSubType.TOKEN_SUBTYPE_INCLUDE: self._hs_include,
            TokenSubType.TOKEN_SUBTYPE_INCLUDEONLY: self._hs_includeonly,
            TokenSubType.TOKEN_SUBTYPE_ONLYINCLUDE: self._hs_onlyinclude,
            TokenSubType.TOKEN_SUBTYPE_TRANSLATE: self._hs_translate,
            TokenSubType.TOKEN_SUBTYPE_ARG: self._hs_arg,
            TokenSubType.TOKEN_SUBTYPE_ARG_NAME: self._hs_arg_name,
            TokenSubType.TOKEN_SUBTYPE_ARG_DEFAULT: self._hs_arg_default,
            TokenSubType.TOKEN_SUBTYPE_TEMPLATE: self._hs_template,
            TokenSubType.TOKEN_SUBTYPE_MAGIC_WORD: self._hs_magic_word,
            TokenSubType.TOKEN_SUBTYPE_MAGIC_WORD_NAME: self._hs_magic_word_name,
            TokenSubType.TOKEN_SUBTYPE_PARAMETER: self._hs_parameter,
            TokenSubType.TOKEN_SUBTYPE_PARAMETER_KEY: self._hs_parameter_key,
            TokenSubType.TOKEN_SUBTYPE_PARAMETER_VALUE: self._hs_parameter_value,
            TokenSubType.TOKEN_SUBTYPE_HEADING: self._hs_heading,
            TokenSubType.TOKEN_SUBTYPE_HEADING_TITLE: self._hs_heading_title,
            TokenSubType.TOKEN_SUBTYPE_HEADING_TRAIL: self._hs_heading_trail,
            TokenSubType.TOKEN_SUBTYPE_HTML: self._hs_html,
            TokenSubType.TOKEN_SUBTYPE_HTML_ATTRS: self._hs_html_attrs,
            TokenSubType.TOKEN_SUBTYPE_HTML_ATTR: self._hs_html_attr,
            TokenSubType.TOKEN_SUBTYPE_HTML_ATTR_DIRTY: self._hs_html_attr_dirty,
            TokenSubType.TOKEN_SUBTYPE_TABLE: self._hs_table,
            TokenSubType.TOKEN_SUBTYPE_TR: self._hs_tr,
            TokenSubType.TOKEN_SUBTYPE_TD: self._hs_td,
            TokenSubType.TOKEN_SUBTYPE_TABLE_SYNTAX: self._hs_table_syntax,
            TokenSubType.TOKEN_SUBTYPE_TABLE_ATTRS: self._hs_table_attrs,
            TokenSubType.TOKEN_SUBTYPE_TABLE_ATTR: self._hs_table_attr,
            TokenSubType.TOKEN_SUBTYPE_TABLE_ATTR_DIRTY: self._hs_table_attr_dirty,
            TokenSubType.TOKEN_SUBTYPE_TABLE_INTER: self._hs_table_inter,
            TokenSubType.TOKEN_SUBTYPE_TABLE_INNER: self._hs_table_inner,
            TokenSubType.TOKEN_SUBTYPE_TD_INNER: self._hs_td_inner,
            TokenSubType.TOKEN_SUBTYPE_HR: self._hs_hr,
            TokenSubType.TOKEN_SUBTYPE_DOUBLE_UNDERSCORE: self._hs_double_underscore,
            TokenSubType.TOKEN_SUBTYPE_LINK: self._hs_link,
            TokenSubType.TOKEN_SUBTYPE_FILE: self._hs_file,
            TokenSubType.TOKEN_SUBTYPE_CATEGORY: self._hs_category,
            TokenSubType.TOKEN_SUBTYPE_TEXT: self._hs_text,
            TokenSubType.TOKEN_SUBTYPE_LINK_TARGET: self._hs_link_target,
            TokenSubType.TOKEN_SUBTYPE_LINK_TEXT: self._hs_link_text,
            TokenSubType.TOKEN_SUBTYPE_QUOTE: self._hs_quote,
            TokenSubType.TOKEN_SUBTYPE_EXT_LINK: self._hs_ext_link,
            TokenSubType.TOKEN_SUBTYPE_EXT_LINK_URL: self._hs_ext_link_url,
            TokenSubType.TOKEN_SUBTYPE_EXT_LINK_TEXT: self._hs_ext_link_text,
            TokenSubType.TOKEN_SUBTYPE_MAGIC_LINK: self._hs_magic_link,
            TokenSubType.TOKEN_SUBTYPE_FREE_EXT_LINK: self._hs_free_ext_link,
            TokenSubType.TOKEN_SUBTYPE_LIST: self._hs_list,
            TokenSubType.TOKEN_SUBTYPE_DD: self._hs_dd,
            TokenSubType.TOKEN_SUBTYPE_CONVERTER: self._hs_converter,
            TokenSubType.TOKEN_SUBTYPE_CONVERTER_RULE: self._hs_converter_rule,
            TokenSubType.TOKEN_SUBTYPE_CONVERTER_RULE_FROM: self._hs_converter_rule_from,
            TokenSubType.TOKEN_SUBTYPE_CONVERTER_RULE_VARIANT: self._hs_converter_rule_variant,
            TokenSubType.TOKEN_SUBTYPE_CONVERTER_RULE_TO: self._hs_converter_rule_to,
            TokenSubType.TOKEN_SUBTYPE_CONVERTER_FLAGS: self._hs_converter_flags,
            TokenSubType.TOKEN_SUBTYPE_CONVERTER_FLAG: self._hs_converter_flag,
            TokenSubType.TOKEN_SUBTYPE_ATTRIBUTES: self._hs_attributes,
            TokenSubType.TOKEN_SUBTYPE_ATTR_EQUAL_TMP: self._hs_attr_equal_tmp,
            TokenSubType.TOKEN_SUBTYPE_ATTR_KEY: self._hs_attr_key,
            TokenSubType.TOKEN_SUBTYPE_ATTR_VALUE: self._hs_attr_value,
            TokenSubType.TOKEN_SUBTYPE_ATOM: self._hs_atom,
            TokenSubType.TOKEN_SUBTYPE_HIDDEN: self._hs_hidden,
            TokenSubType.TOKEN_SUBTYPE_EXT_ATTRS: self._hs_ext_attrs,
            TokenSubType.TOKEN_SUBTYPE_EXT_INNER: self._hs_ext_inner,
            TokenSubType.TOKEN_SUBTYPE_EXT_ATTR_DIRTY: self._hs_ext_attr_dirty,
            TokenSubType.TOKEN_SUBTYPE_EXT_ATTR: self._hs_ext_attr,
            TokenSubType.TOKEN_SUBTYPE_IMAGE_PARAMETER: self._hs_image_parameter,
            TokenSubType.TOKEN_SUBTYPE_GALLERY_IMAGE: self._hs_gallery_image,
            TokenSubType.TOKEN_SUBTYPE_IMAGEMAP_IMAGE: self._hs_imagemap_image,
            TokenSubType.TOKEN_SUBTYPE_GALLERY_LINE: self._hs_gallery_line,
            TokenSubType.TOKEN_SUBTYPE_GALLERY_PARAM_WRAPPER: self._hs_gallery_param_wrapper,
            TokenSubType.TOKEN_SUBTYPE_IMAGEMAP_LINK_INNER: self._hs_imagemap_link_inner,
            TokenSubType.TOKEN_SUBTYPE_IMAGEMAP_IMAGE_LINE: self._hs_imagemap_image_line,
            TokenSubType.TOKEN_SUBTYPE_IMAGEMAP_LINK: self._hs_imagemap_link,
            TokenSubType.TOKEN_SUBTYPE_TEMPLATE_NAME: self._hs_template_name,
            TokenSubType.TOKEN_SUBTYPE_INVOKE_MODULE: self._hs_invoke_module,
            TokenSubType.TOKEN_SUBTYPE_INVOKE_FUNCTION: self._hs_invoke_function,
            TokenSubType.TOKEN_SUBTYPE_PARAM_LINE: self._hs_param_line,
        }

    def convert(self, root_token):
        self._process_token(root_token)
        self._emit_references_if_needed()
        return "".join(self.output)

    def _process_token(self, token):
        if token is None:
            return
        # Dispatch policy: semantic subtype first, then structural type fallback.
        subtype = getattr(token, "subtype", TokenSubType.TOKEN_SUBTYPE_NONE)
        subtype_handler = self.subtype_handlers.get(subtype)
        if subtype_handler is not None:
            subtype_handler(token)
            return

        type_handler = self.type_handlers.get(token.type, self._h_passthrough)
        type_handler(token)

    def _children(self, token):
        for child in token.children:
            yield child

    def _token_children(self, token):
        for child in token.children:
            if hasattr(child, "type"):
                yield child

    def _process_children(self, token):
        for child in self._children(token):
            if isinstance(child, str):
                self.output.append(child)
            else:
                self._process_token(child)

    def _extract_text(self, token):
        parts = []
        for child in self._children(token):
            if isinstance(child, str):
                parts.append(child)
            else:
                parts.append(self._extract_text(child))
        return "".join(parts)

    def _wiki_target_url(self, target):
        if not target:
            return ""
        t = target.strip()
        if not t:
            return ""
        if "://" in t:
            return t
        # MediaWiki page paths are underscore-separated and URL-encoded.
        page = t.replace(" ", "_")
        return WIKI_ROOT_URL + quote(page, safe="():'%")

    def _raw_from_token(self, token):
        # Raw-ish serializer for template code output.
        if token is None:
            return ""
        if token.type == TokenType.TOKEN_TRANSCLUDE and token.subtype == TokenSubType.TOKEN_SUBTYPE_TEMPLATE:
            name = ""
            params = []
            for child in self._token_children(token):
                if child.subtype == TokenSubType.TOKEN_SUBTYPE_TEMPLATE_NAME:
                    name = self._extract_text(child)
                elif child.type == TokenType.TOKEN_PARAMETER or child.subtype == TokenSubType.TOKEN_SUBTYPE_PARAMETER:
                    params.append(self._raw_parameter(child))
            if params:
                return "{{" + name + "|" + "|".join(params) + "}}"
            return "{{" + name + "}}"
        return self._extract_text(token)

    def _raw_parameter(self, token):
        key = None
        value = ""
        for child in self._token_children(token):
            if child.subtype == TokenSubType.TOKEN_SUBTYPE_PARAMETER_KEY:
                key = self._extract_text(child)
            elif child.subtype == TokenSubType.TOKEN_SUBTYPE_PARAMETER_VALUE:
                value = self._extract_text(child)
        if key is None:
            return value
        return key + "=" + value

    def _extract_ext_attributes(self, token):
        attrs = {}
        for child in self._token_children(token):
            if child.subtype != TokenSubType.TOKEN_SUBTYPE_EXT_ATTRS:
                continue
            for attr in self._token_children(child):
                if attr.subtype != TokenSubType.TOKEN_SUBTYPE_EXT_ATTR:
                    continue
                key = (getattr(attr, "name", "") or "").strip().lower()
                val = ""
                for attr_child in self._token_children(attr):
                    if attr_child.subtype == TokenSubType.TOKEN_SUBTYPE_ATTR_KEY and not key:
                        key = self._extract_text(attr_child).strip().lower()
                    elif attr_child.subtype == TokenSubType.TOKEN_SUBTYPE_ATTR_VALUE:
                        val = self._extract_text(attr_child).strip()
                if key:
                    attrs[key] = val
        return attrs

    def _extract_ext_inner_text(self, token):
        for child in self._token_children(token):
            if child.subtype == TokenSubType.TOKEN_SUBTYPE_EXT_INNER:
                saved = self.output
                temp = []
                self.output = temp
                self._process_token(child)
                self.output = saved
                return "".join(temp).strip()
        return ""

    def _make_ref_id(self, ref_name):
        if ref_name:
            safe = []
            for c in ref_name:
                if c.isalnum() or c in "-_":
                    safe.append(c)
                elif c.isspace():
                    safe.append("-")
            ref_id = "".join(safe).strip("-")
            if ref_id:
                return ref_id
        return str(len(self.footnotes) + 1)

    def _register_reference(self, ref_name, content):
        if ref_name and ref_name in self.footnote_index_by_name:
            idx = self.footnote_index_by_name[ref_name]
            if content and not self.footnotes[idx]["content"]:
                self.footnotes[idx]["content"] = content
            return self.footnotes[idx]["id"]

        ref_id = self._make_ref_id(ref_name)
        used = {item["id"] for item in self.footnotes}
        if ref_id in used:
            n = 2
            base = ref_id
            while f"{base}-{n}" in used:
                n += 1
            ref_id = f"{base}-{n}"

        idx = len(self.footnotes)
        self.footnotes.append({"id": ref_id, "content": content})
        if ref_name:
            self.footnote_index_by_name[ref_name] = idx
        return ref_id

    def _emit_references_if_needed(self):
        if self.references_emitted:
            return
        visible = [f for f in self.footnotes if f["content"]]
        if not visible:
            self.references_emitted = True
            return
        self.output.append("\n\n## References\n\n")
        for item in visible:
            self.output.append(f"[^{item['id']}]: {item['content']}\n")
        self.references_emitted = True

    # Subtype handlers (one method per subtype for debug visibility)
    def _hs_none(self, token):
        self._h_subtype_none(token)

    def _hs_root(self, token):
        self._process_children(token)

    def _hs_redirect(self, token):
        self._h_redirect(token)

    def _hs_redirect_syntax(self, token):
        # The '#REDIRECT' keyword text — not content.
        self._h_skip(token)

    def _hs_redirect_target(self, token):
        # Child of redirect (which is already skipped).
        self._h_skip(token)

    def _hs_comment(self, token):
        self._h_comment(token)

    def _hs_ext(self, token):
        self._h_ext(token)

    def _hs_noinclude(self, token):
        # In page-view mode, noinclude tokens commonly represent raw tag syntax.
        # Skip the tag token itself.
        self._h_skip(token)

    def _hs_include(self, token):
        # <includeonly>: content is NOT shown when viewing the page, only when transcluded.
        self._h_skip(token)

    def _hs_includeonly(self, token):
        # <includeonly>: same as above.
        self._h_skip(token)

    def _hs_onlyinclude(self, token):
        self._h_include_like(token)

    def _hs_translate(self, token):
        self._h_translate(token)

    def _hs_arg(self, token):
        # Template argument placeholders {{{1}}} — no markdown equivalent.
        self._h_skip(token)

    def _hs_arg_name(self, token):
        # Internal to arg (which is skipped).
        self._h_skip(token)

    def _hs_arg_default(self, token):
        # Internal to arg (which is skipped).
        self._h_skip(token)

    def _hs_template(self, token):
        self._h_template_subtype(token)

    def _hs_magic_word(self, token):
        # {{PAGENAME}}, {{#if:}}, etc. — no meaningful markdown output.
        self._h_skip(token)

    def _hs_magic_word_name(self, token):
        # Internal name node inside magic-word (which is skipped).
        self._h_skip(token)

    def _hs_parameter(self, token):
        # Template parameters — only appear inside templates (JSON-encoded) or magic-words (skipped).
        self._h_skip(token)

    def _hs_parameter_key(self, token):
        # Internal to parameter (which is skipped).
        self._h_skip(token)

    def _hs_parameter_value(self, token):
        # Internal to parameter (which is skipped).
        self._h_skip(token)

    def _hs_heading(self, token):
        self._h_heading(token)

    def _hs_heading_title(self, token):
        self._h_passthrough(token)

    def _hs_heading_trail(self, token):
        # Trailing == markers and whitespace after heading text — heading handler already adds \n\n.
        self._h_skip(token)

    def _hs_html(self, token):
        self._h_html(token)

    def _hs_html_attrs(self, token):
        self._h_skip(token)

    def _hs_html_attr(self, token):
        self._h_skip(token)

    def _hs_html_attr_dirty(self, token):
        self._h_skip(token)

    def _hs_table(self, token):
        self._h_table(token)

    def _hs_tr(self, token):
        self._h_tr(token)

    def _hs_td(self, token):
        self._h_td(token)

    def _hs_table_syntax(self, token):
        self._h_skip(token)

    def _hs_table_attrs(self, token):
        self._h_skip(token)

    def _hs_table_attr(self, token):
        self._h_skip(token)

    def _hs_table_attr_dirty(self, token):
        self._h_skip(token)

    def _hs_table_inter(self, token):
        self._h_passthrough(token)

    def _hs_table_inner(self, token):
        self._h_passthrough(token)

    def _hs_td_inner(self, token):
        self._h_passthrough(token)

    def _hs_hr(self, token):
        self._h_hr(token)

    def _hs_double_underscore(self, token):
        # __TOC__, __NOTOC__, __FORCETOC__ etc. are control words with no markdown equivalent.
        self._h_skip(token)

    def _hs_link(self, token):
        self._h_link(token)

    def _hs_file(self, token):
        self._h_file(token)

    def _hs_category(self, token):
        self._h_category(token)

    def _hs_text(self, token):
        self._h_text(token)

    def _hs_link_target(self, token):
        self._h_passthrough(token)

    def _hs_link_text(self, token):
        self._h_passthrough(token)

    def _hs_quote(self, token):
        self._h_quote(token)

    def _hs_ext_link(self, token):
        self._h_ext_link(token)

    def _hs_ext_link_url(self, token):
        self._h_passthrough(token)

    def _hs_ext_link_text(self, token):
        self._h_passthrough(token)

    def _hs_magic_link(self, token):
        self._h_magic_link(token)

    def _hs_free_ext_link(self, token):
        self._h_magic_link(token)

    def _hs_list(self, token):
        self._h_list(token)

    def _hs_dd(self, token):
        self._h_dd(token)

    def _hs_converter(self, token):
        # Language variant converter -{...}- — no markdown equivalent.
        self._h_skip(token)

    def _hs_converter_rule(self, token):
        # Internal to converter.
        self._h_skip(token)

    def _hs_converter_rule_from(self, token):
        # Internal to converter.
        self._h_skip(token)

    def _hs_converter_rule_variant(self, token):
        # Internal to converter.
        self._h_skip(token)

    def _hs_converter_rule_to(self, token):
        # Internal to converter.
        self._h_skip(token)

    def _hs_converter_flags(self, token):
        # Internal to converter.
        self._h_skip(token)

    def _hs_converter_flag(self, token):
        # Internal to converter.
        self._h_skip(token)

    def _hs_attributes(self, token):
        # HTML/table attribute container — attributes are not emitted in markdown.
        self._h_skip(token)

    def _hs_attr_equal_tmp(self, token):
        # Transient attribute = separator — not content.
        self._h_skip(token)

    def _hs_attr_key(self, token):
        # HTML attribute key — not content.
        self._h_skip(token)

    def _hs_attr_value(self, token):
        # HTML attribute value — not content.
        self._h_skip(token)

    def _hs_atom(self, token):
        self._h_passthrough(token)

    def _hs_hidden(self, token):
        self._h_skip(token)

    def _hs_ext_attrs(self, token):
        self._h_skip(token)

    def _hs_ext_inner(self, token):
        self._h_passthrough(token)

    def _hs_ext_attr_dirty(self, token):
        self._h_skip(token)

    def _hs_ext_attr(self, token):
        self._h_skip(token)

    def _hs_image_parameter(self, token):
        self._h_image_parameter(token)

    def _hs_gallery_image(self, token):
        self._h_file(token)

    def _hs_imagemap_image(self, token):
        self._h_file(token)

    def _hs_gallery_line(self, token):
        self._h_passthrough(token)

    def _hs_gallery_param_wrapper(self, token):
        # Gallery settings wrapper (mode=packed, widths=200, etc.) — metadata, not content.
        self._h_skip(token)

    def _hs_imagemap_link_inner(self, token):
        self._h_passthrough(token)

    def _hs_imagemap_image_line(self, token):
        self._h_passthrough(token)

    def _hs_imagemap_link(self, token):
        self._h_passthrough(token)

    def _hs_template_name(self, token):
        # Template name node — internal to template which is JSON-encoded as a whole.
        self._h_skip(token)

    def _hs_invoke_module(self, token):
        # Lua module name — internal to template JSON.
        self._h_skip(token)

    def _hs_invoke_function(self, token):
        # Lua function name — internal to template JSON.
        self._h_skip(token)

    def _hs_param_line(self, token):
        # Gallery/imagemap config lines (widths=200px etc.) — metadata, not content.
        self._h_skip(token)

    def _h_text(self, token):
        self._process_children(token)

    def _h_plain(self, token):
        self._process_children(token)

    def _h_comment(self, token):
        _ = token

    def _h_subtype_none(self, token):
        type_handler = self.type_handlers.get(token.type, self._h_passthrough)
        type_handler(token)

    def _h_skip(self, token):
        _ = token

    def _h_passthrough(self, token):
        self._process_children(token)

    def _h_heading(self, token):
        level = token.level if token.level is not None else 2
        level = max(1, min(6, int(level)))
        self.output.append("\n" + ("#" * level) + " ")
        self._process_children(token)
        self.output.append("\n\n")

    def _h_link(self, token):
        target = ""
        text = ""
        for child in self._token_children(token):
            if child.subtype == TokenSubType.TOKEN_SUBTYPE_LINK_TARGET:
                target = self._extract_text(child)
            elif child.subtype == TokenSubType.TOKEN_SUBTYPE_LINK_TEXT:
                text = self._extract_text(child)
        if not target:
            self._process_children(token)
            return
        if not text:
            text = target
        self.output.append(f"[{text}]({self._wiki_target_url(target)})")

    def _h_ext_link(self, token):
        url = ""
        text = ""
        for child in self._token_children(token):
            if child.subtype == TokenSubType.TOKEN_SUBTYPE_EXT_LINK_URL:
                url = self._extract_text(child)
            elif child.subtype == TokenSubType.TOKEN_SUBTYPE_EXT_LINK_TEXT:
                text = self._extract_text(child)
        if not url:
            self._process_children(token)
            return
        if not text:
            text = url
        self.output.append(f"[{text}]({url})")

    def _h_magic_link(self, token):
        text = self._extract_text(token)
        self.output.append(text)

    def _h_file(self, token):
        target = ""
        caption = ""
        for child in self._token_children(token):
            if child.subtype == TokenSubType.TOKEN_SUBTYPE_LINK_TARGET and not target:
                target = self._extract_text(child)
            elif child.subtype == TokenSubType.TOKEN_SUBTYPE_IMAGE_PARAMETER:
                val = child.image_param_raw_syntax if hasattr(child, "image_param_raw_syntax") else None
                if val and val.strip() and not caption:
                    caption = val.strip()

        if not target:
            return

        clean = target.replace("File:", "").replace("Image:", "")
        alt = caption if caption else clean
        self.output.append(f"![{alt}]({clean})")

    def _h_category(self, token):
        _ = token

    def _h_redirect(self, token):
        target = getattr(token, "redirect_link", None)
        if target:
            self.output.append(f"[Redirect: {target}]({self._wiki_target_url(target)})\n")
            return
        self._h_skip(token)

    # TODO:  Should the bold and italic tokens contain the text that depicts it is bold/itelic as a value instead of having a child token?
    def _h_quote(self, token):
        # Quote tokens are markers (''', '', '''''), not wrappers
        # The text between opening and closing markers needs to be wrapped
        # Since the parser doesn't create a wrapper structure, we need to track state
        
        # For now, just output the appropriate markdown marker
        # The opening and closing markers will both trigger this handler
        bold = bool(token.quote_bold)
        italic = bool(token.quote_italic)
        if bold and italic:
            mark = "***"
        elif bold:
            mark = "**"
        elif italic:
            mark = "*"
        else:
            mark = ""
        # Don't process children (which would output ''', ''', etc.)
        # Just output the markdown marker
        self.output.append(mark)

    def _h_html(self, token):
        tag = (token.html_orig_tag or token.name or "span").strip().strip('"').lower()

        if token.html_self_closing:
            self.output.append(f"<{tag}/>")
            return
        if token.html_closing:
            self.output.append(f"</{tag}>")
            return
        self.output.append(f"<{tag}>")
        self._process_children(token)
        self.output.append(f"</{tag}>")

    def _h_ext(self, token):
        name = token.ext_name or token.name or "ext"
        lower = name.strip().lower()
        attrs = self._extract_ext_attributes(token)

        if lower == "ref":
            ref_name = attrs.get("name")
            content = ""
            if not token.ext_self_closing:
                content = self._extract_ext_inner_text(token)
            ref_id = self._register_reference(ref_name, content)
            self.output.append(f"[^{ref_id}]")
            return

        if lower == "references":
            self._emit_references_if_needed()
            return

        if lower == "nowiki":
            self.output.append(self._extract_text(token))
            return

        if lower in {"pre", "source", "syntaxhighlight"}:
            code = self._extract_ext_inner_text(token)
            lang = attrs.get("lang", "")
            self.output.append("\n```" + lang + "\n" + code + "\n```\n")
            return

        if lower in {"math", "chem", "ce"}:
            math_text = self._extract_ext_inner_text(token)
            if "\n" in math_text:
                self.output.append("\n$$\n" + math_text + "\n$$\n")
            else:
                self.output.append("$" + math_text + "$")
            return

        if lower == "poem":
            poem = self._extract_ext_inner_text(token)
            self.output.append("\n" + poem + "\n")
            return

        if lower in {"gallery", "imagemap", "mapframe", "maplink", "score", "phonos", "timeline", "graph", "hiero"}:
            label = self._extract_ext_inner_text(token)
            if label:
                self.output.append(f"[unsupported ext: {name}] " + label)
            else:
                self.output.append(f"[unsupported ext: {name}]")
            return

        if lower in {"indicator", "inputbox", "categorytree", "templatestyles", "templatedata", "section", "page-collection", "charinsert", "langconvert"}:
            return

        if token.ext_self_closing:
            self.output.append(f"<{name}/>")
            return
        self.output.append(f"<{name}>")
        self._process_children(token)
        self.output.append(f"</{name}>")

    def _h_include_like(self, token):
        self._process_children(token)

    def _h_translate(self, token):
        self._process_children(token)

    def _h_arg(self, token):
        self.output.append("{{{")
        self._process_children(token)
        self.output.append("}}}")

    def _h_transclude(self, token):
        # Non-template transclude forms (e.g., magic words) continue as markdown text.
        self._process_children(token)

    def _h_template_subtype(self, token):
        # Templates are preserved as JSON payload for now.
        try:
            payload = token.to_json(False)
        except Exception:
            payload = self._raw_from_token(token)
        self.output.append("`" + payload + "`")

    def _h_magic_word_subtype(self, token):
        raw = self._extract_text(token)
        self.output.append(raw)

    def _h_parameter(self, token):
        self._process_children(token)

    def _h_table(self, token):
        self.table_rows = []
        self._process_children(token)
        if self.table_rows:
            header = self.table_rows[0]
            self.output.append("\n| " + " | ".join(header) + " |\n")
            self.output.append("| " + " | ".join(["---"] * len(header)) + " |\n")
            for row in self.table_rows[1:]:
                self.output.append("| " + " | ".join(row) + " |\n")
            self.output.append("\n")
        self.table_rows = []

    def _h_tr(self, token):
        self.current_row = []
        self._process_children(token)
        if self.current_row:
            self.table_rows.append(self.current_row)

    def _h_td(self, token):
        saved = self.output
        cell_out = []
        self.output = cell_out
        self._process_children(token)
        self.output = saved
        self.current_row.append("".join(cell_out).strip())

    def _h_list(self, token):
        self.list_level += 1
        indent = "  " * (self.list_level - 1)
        marker = "* "
        for child in self._children(token):
            if isinstance(child, str):
                stripped = child.lstrip()
                if stripped.startswith("#"):
                    marker = "1. "
                break
        self.output.append(indent + marker)
        self._process_children(token)
        #self.output.append("\n")
        self.list_level -= 1

    def _h_dd(self, token):
        self.output.append(": ")
        self._process_children(token)
        self.output.append("\n")

    def _h_hr(self, token):
        _ = token
        self.output.append("\n---\n")

    def _h_double_underscore(self, token):
        self.output.append(token.name or "")

    def _h_image_parameter(self, token):
        # Metadata for files; only emit if it carries visible caption-like text.
        raw = token.image_param_raw_syntax if hasattr(token, "image_param_raw_syntax") else None
        if raw and raw.strip():
            self.output.append(raw)
            return
        self._process_children(token)


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    config_path = os.path.join(script_dir, "..", "..", "..", "config", "enwiki.json")
    config_path = os.path.normpath(config_path)

    try:
        config = WikiConfig.from_file(config_path)
    except Exception as exc:
        print(f"Warning: Could not load config from {config_path}: {exc}", file=sys.stderr)
        config = WikiConfig.from_string(
            '{"ext": ["ref"], "html": [[],[],[]], "protocol": "https?://|http://"}'
        )

    # Instead of stdin, we should read from a file.
    input_file = os.path.join(script_dir, "..", "..", "..", "compare", "test-data", "wikitext", "Achilles.wikitext")
    try:
        with open(input_file, "r", encoding="utf-8") as f:
            wikitext = f.read()
    except Exception as exc:
        print(f"Error reading input file {input_file}: {exc}", file=sys.stderr)
        sys.exit(1)

    if not wikitext.strip():
        print("No input provided", file=sys.stderr)
        sys.exit(1)

    parser = WikiParser(config)

    try:
        root = parser.parse(wikitext)
        # root = parser.parse_to_dict(wikitext)
        
        converter = MarkdownConverter()
        print(converter.convert(root))
    except Exception as exc:
        print(f"Error parsing wikitext: {exc}", file=sys.stderr)
        import traceback

        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()