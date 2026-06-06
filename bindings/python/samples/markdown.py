#!/usr/bin/env python3
"""
WikiCast to Markdown converter.

Reads wikitext from stdin, parses using parser.parse(), and writes Markdown to stdout.

This sample needs work!!!!!
"""

import os
import sys

from wiki_cast import WikiConfig, WikiParser, TokenSubType, TokenType


class MarkdownConverter:
    def __init__(self):
        self.output = []
        self.list_level = 0
        self.table_rows = []
        self.current_row = []

    def convert(self, root_token):
        self._process_token(root_token)
        return "".join(self.output)

    def _process_token(self, token):
        if token is None:
            return

        type_handlers = {
            TokenType.TOKEN_TEXT: self._h_text,
            TokenType.TOKEN_ROOT: self._h_root,
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

        subtype_handlers = {
            TokenSubType.TOKEN_SUBTYPE_TEMPLATE: self._h_template_subtype,
            TokenSubType.TOKEN_SUBTYPE_TEMPLATE_NAME: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_MAGIC_WORD: self._h_magic_word_subtype,
            TokenSubType.TOKEN_SUBTYPE_MAGIC_WORD_NAME: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_PARAMETER: self._h_parameter,
            TokenSubType.TOKEN_SUBTYPE_PARAMETER_KEY: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_PARAMETER_VALUE: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_HEADING_TITLE: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_HEADING_TRAIL: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_LINK_TARGET: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_LINK_TEXT: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_EXT_LINK_URL: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_EXT_LINK_TEXT: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_FREE_EXT_LINK: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_TABLE_SYNTAX: self._h_skip,
            TokenSubType.TOKEN_SUBTYPE_TABLE_ATTRS: self._h_skip,
            TokenSubType.TOKEN_SUBTYPE_TABLE_ATTR: self._h_skip,
            TokenSubType.TOKEN_SUBTYPE_TABLE_ATTR_DIRTY: self._h_skip,
            TokenSubType.TOKEN_SUBTYPE_TABLE_INTER: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_TABLE_INNER: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_TD_INNER: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_EXT_ATTRS: self._h_skip,
            TokenSubType.TOKEN_SUBTYPE_EXT_INNER: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_EXT_ATTR_DIRTY: self._h_skip,
            TokenSubType.TOKEN_SUBTYPE_EXT_ATTR: self._h_skip,
            TokenSubType.TOKEN_SUBTYPE_ATTR_KEY: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_ATTR_VALUE: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_IMAGE_PARAMETER: self._h_image_parameter,
            TokenSubType.TOKEN_SUBTYPE_GALLERY_IMAGE: self._h_file,
            TokenSubType.TOKEN_SUBTYPE_IMAGEMAP_IMAGE: self._h_file,
            TokenSubType.TOKEN_SUBTYPE_GALLERY_LINE: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_GALLERY_PARAM_WRAPPER: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_IMAGEMAP_LINK_INNER: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_IMAGEMAP_IMAGE_LINE: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_IMAGEMAP_LINK: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_INVOKE_MODULE: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_INVOKE_FUNCTION: self._h_passthrough,
            TokenSubType.TOKEN_SUBTYPE_PARAM_LINE: self._h_passthrough,
        }

        subtype_handler = subtype_handlers.get(token.subtype)
        if subtype_handler is not None:
            subtype_handler(token)
            return

        type_handler = type_handlers.get(token.type, self._h_passthrough)
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

    # Handlers
    def _h_root(self, token):
        self._process_children(token)

    def _h_text(self, token):
        self._process_children(token)

    def _h_plain(self, token):
        self._process_children(token)

    def _h_comment(self, token):
        _ = token

    def _h_skip(self, token):
        _ = token

    def _h_passthrough(self, token):
        self._process_children(token)

    def _h_heading(self, token):
        level = token.level if token.level is not None else 1
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
        self.output.append(f"[{text}]({target})")

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
        self.output.append("<!-- Redirect -->\n")
        self._process_children(token)

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
        self.output.append(f"<{tag}>")
        self._process_children(token)
        if not token.html_closing:
            self.output.append(f"</{tag}>")

    def _h_ext(self, token):
        name = token.ext_name or token.name or "ext"
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
        # Requirement: template should be single tick and raw data.
        raw = self._raw_from_token(token)
        self.output.append("`" + raw + "`")

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
        self.output.append("\n")
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

    wikitext = sys.stdin.read()
    if not wikitext.strip():
        print("No input provided", file=sys.stderr)
        sys.exit(1)

    parser = WikiParser(config)

    try:
        root = parser.parse(wikitext)
        converter = MarkdownConverter()
        print(converter.convert(root))
    except Exception as exc:
        print(f"Error parsing wikitext: {exc}", file=sys.stderr)
        import traceback

        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()