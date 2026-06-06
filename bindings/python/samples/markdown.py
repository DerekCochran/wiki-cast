#!/usr/bin/env python3
"""
WikiCast to Markdown converter.

Reads wikitext from stdin, parses it using the raw token API,
and writes Markdown to stdout.
"""

import os
import sys

from wiki_cast import WikiConfig, WikiParser, TokenSubType, TokenType


class MarkdownConverter:
    """Converts a WikiCast PyToken tree to Markdown."""

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

        handlers = {
            TokenType.TOKEN_ROOT: self._handle_root,
            TokenType.TOKEN_TEXT: self._handle_text,
            TokenType.TOKEN_HEADING: self._handle_heading,
            TokenType.TOKEN_LINK: self._handle_link,
            TokenType.TOKEN_FILE: self._handle_file,
            TokenType.TOKEN_CATEGORY: self._handle_category,
            TokenType.TOKEN_EXT_LINK: self._handle_ext_link,
            TokenType.TOKEN_HTML: self._handle_html,
            TokenType.TOKEN_TABLE: self._handle_table,
            TokenType.TOKEN_TR: self._handle_tr,
            TokenType.TOKEN_TD: self._handle_td,
            TokenType.TOKEN_HR: self._handle_hr,
            TokenType.TOKEN_QUOTE: self._handle_quote,
            TokenType.TOKEN_EXT: self._handle_ext,
            TokenType.TOKEN_REDIRECT: self._handle_redirect,
            TokenType.TOKEN_ARG: self._handle_arg,
            TokenType.TOKEN_LIST: self._handle_list,
            TokenType.TOKEN_DD: self._handle_dd,
            TokenType.TOKEN_COMMENT: self._handle_comment,
            TokenType.TOKEN_HIDDEN: self._handle_hidden,
            TokenType.TOKEN_DOUBLE_UNDERSCORE: self._handle_double_underscore,
        }

        handler = handlers.get(token.type, self._handle_default)
        handler(token)

    def _iter_token_children(self, token):
        for child in token.children:
            if hasattr(child, "type"):
                yield child

    def _process_children(self, token):
        for child in token.children:
            if isinstance(child, str):
                self.output.append(child)
            else:
                self._process_token(child)

    def _extract_text(self, token):
        parts = []
        for child in token.children:
            if isinstance(child, str):
                parts.append(child)
            else:
                parts.append(self._extract_text(child))
        return "".join(parts)

    def _handle_root(self, token):
        self._process_children(token)

    def _handle_text(self, token):
        self._process_children(token)

    def _handle_heading(self, token):
        level = token.level if token.level is not None else 1
        level = max(1, min(6, int(level)))
        self.output.append("\n" + ("#" * level) + " ")
        self._process_children(token)
        self.output.append("\n\n")

    def _handle_link(self, token):
        target = ""
        text = ""

        for child in self._iter_token_children(token):
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

    def _handle_ext_link(self, token):
        url = ""
        text = ""

        for child in self._iter_token_children(token):
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

    def _handle_file(self, token):
        file_name = ""
        for child in self._iter_token_children(token):
            if child.subtype == TokenSubType.TOKEN_SUBTYPE_LINK_TARGET:
                file_name = self._extract_text(child)
                break

        if not file_name:
            return

        file_name = file_name.replace("File:", "").replace("Image:", "")
        self.output.append(f"![{file_name}]({file_name})")

    def _handle_category(self, token):
        _ = token
        return

    def _handle_html(self, token):
        tag = token.html_orig_tag or token.name or "span"
        if token.html_self_closing:
            self.output.append(f"<{tag}/>")
            return

        self.output.append(f"<{tag}>")
        self._process_children(token)
        if not token.html_closing:
            self.output.append(f"</{tag}>")

    def _handle_table(self, token):
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

    def _handle_tr(self, token):
        self.current_row = []
        self._process_children(token)
        if self.current_row:
            self.table_rows.append(self.current_row)

    def _handle_td(self, token):
        saved = self.output
        cell_out = []
        self.output = cell_out
        self._process_children(token)
        self.output = saved
        self.current_row.append("".join(cell_out).strip())

    def _handle_hr(self, token):
        _ = token
        self.output.append("\n---\n")

    def _handle_quote(self, token):
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

        self.output.append(mark)
        self._process_children(token)
        self.output.append(mark)

    def _handle_ext(self, token):
        name = token.ext_name or token.name or "ext"
        if token.ext_self_closing:
            self.output.append(f"<{name}/>")
            return

        self.output.append(f"<{name}>")
        self._process_children(token)
        self.output.append(f"</{name}>")

    def _handle_redirect(self, token):
        self.output.append("<!-- Redirect -->\n")
        self._process_children(token)

    def _handle_arg(self, token):
        self.output.append("{{")
        self._process_children(token)
        self.output.append("}}")

    def _handle_list(self, token):
        self.list_level += 1
        indent = "  " * (self.list_level - 1)

        marker = "* "
        for child in token.children:
            if isinstance(child, str):
                trimmed = child.lstrip()
                if trimmed.startswith("#"):
                    marker = "1. "
                break

        self.output.append(indent + marker)
        self._process_children(token)
        self.output.append("\n")
        self.list_level -= 1

    def _handle_dd(self, token):
        self.output.append(": ")
        self._process_children(token)
        self.output.append("\n")

    def _handle_comment(self, token):
        _ = token
        return

    def _handle_hidden(self, token):
        _ = token
        return

    def _handle_double_underscore(self, token):
        name = token.name or ""
        self.output.append(f"`{name}`")

    def _handle_default(self, token):
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
        markdown = converter.convert(root)
        print(markdown)
    except Exception as exc:
        print(f"Error parsing wikitext: {exc}", file=sys.stderr)
        import traceback

        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()