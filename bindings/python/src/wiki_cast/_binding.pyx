# cython: language_level=3
import json
from libc.stdlib cimport free


# 2. Forward-declare the exact C structures from token.h and parse.h
cdef extern from "wiki_cast/token.h":
    # 1. Map the C Enums so they are exposed natively to Python
    cpdef enum TokenType:
        TOKEN_TEXT
        TOKEN_ROOT
        TOKEN_PLAIN
        TOKEN_COMMENT
        TOKEN_EXT
        TOKEN_NOINCLUDE
        TOKEN_INCLUDE
        TOKEN_TRANSLATE
        TOKEN_ONLYINCLUDE
        TOKEN_ARG
        TOKEN_TRANSCLUDE
        TOKEN_HEADING
        TOKEN_HTML
        TOKEN_TABLE
        TOKEN_TR
        TOKEN_TD
        TOKEN_HR
        TOKEN_DOUBLE_UNDERSCORE
        TOKEN_LINK
        TOKEN_FILE
        TOKEN_CATEGORY
        TOKEN_REDIRECT
        TOKEN_REDIRECT_TARGET
        TOKEN_REDIRECT_SYNTAX
        TOKEN_QUOTE
        TOKEN_EXT_LINK
        TOKEN_MAGIC_LINK
        TOKEN_LIST
        TOKEN_DD
        TOKEN_CONVERTER
        TOKEN_PARAMETER
        TOKEN_ATTRIBUTES
        TOKEN_SYNTAX
        TOKEN_ATOM
        TOKEN_HIDDEN
        TOKEN_EXT_ATTRS
        TOKEN_EXT_INNER
        TOKEN_EXT_ATTR_DIRTY
        TOKEN_EXT_ATTR
        TOKEN_ATTR_KEY
        TOKEN_ATTR_VALUE
        TOKEN_TYPE_COUNT

    cpdef enum TokenSubType:
        TOKEN_SUBTYPE_NONE
        TOKEN_SUBTYPE_ROOT
        TOKEN_SUBTYPE_REDIRECT
        TOKEN_SUBTYPE_REDIRECT_SYNTAX
        TOKEN_SUBTYPE_REDIRECT_TARGET
        TOKEN_SUBTYPE_COMMENT
        TOKEN_SUBTYPE_EXT
        TOKEN_SUBTYPE_NOINCLUDE
        TOKEN_SUBTYPE_INCLUDE
        TOKEN_SUBTYPE_INCLUDEONLY
        TOKEN_SUBTYPE_ONLYINCLUDE
        TOKEN_SUBTYPE_TRANSLATE
        TOKEN_SUBTYPE_ARG
        TOKEN_SUBTYPE_ARG_NAME
        TOKEN_SUBTYPE_ARG_DEFAULT
        TOKEN_SUBTYPE_TEMPLATE
        TOKEN_SUBTYPE_MAGIC_WORD
        TOKEN_SUBTYPE_MAGIC_WORD_NAME
        TOKEN_SUBTYPE_PARAMETER
        TOKEN_SUBTYPE_PARAMETER_KEY
        TOKEN_SUBTYPE_PARAMETER_VALUE
        TOKEN_SUBTYPE_HEADING
        TOKEN_SUBTYPE_HEADING_TITLE
        TOKEN_SUBTYPE_HEADING_TRAIL
        TOKEN_SUBTYPE_HTML
        TOKEN_SUBTYPE_HTML_ATTRS
        TOKEN_SUBTYPE_HTML_ATTR
        TOKEN_SUBTYPE_HTML_ATTR_DIRTY
        TOKEN_SUBTYPE_TABLE
        TOKEN_SUBTYPE_TR
        TOKEN_SUBTYPE_TD
        TOKEN_SUBTYPE_TABLE_SYNTAX
        TOKEN_SUBTYPE_TABLE_ATTRS
        TOKEN_SUBTYPE_TABLE_ATTR
        TOKEN_SUBTYPE_TABLE_ATTR_DIRTY
        TOKEN_SUBTYPE_TABLE_INTER
        TOKEN_SUBTYPE_TABLE_INNER
        TOKEN_SUBTYPE_TD_INNER
        TOKEN_SUBTYPE_HR
        TOKEN_SUBTYPE_DOUBLE_UNDERSCORE
        TOKEN_SUBTYPE_LINK
        TOKEN_SUBTYPE_FILE
        TOKEN_SUBTYPE_CATEGORY
        TOKEN_SUBTYPE_TEXT
        TOKEN_SUBTYPE_LINK_TARGET
        TOKEN_SUBTYPE_LINK_TEXT
        TOKEN_SUBTYPE_QUOTE
        TOKEN_SUBTYPE_EXT_LINK
        TOKEN_SUBTYPE_EXT_LINK_URL
        TOKEN_SUBTYPE_EXT_LINK_TEXT
        TOKEN_SUBTYPE_MAGIC_LINK
        TOKEN_SUBTYPE_FREE_EXT_LINK
        TOKEN_SUBTYPE_LIST
        TOKEN_SUBTYPE_DD
        TOKEN_SUBTYPE_CONVERTER
        TOKEN_SUBTYPE_CONVERTER_RULE
        TOKEN_SUBTYPE_CONVERTER_RULE_FROM
        TOKEN_SUBTYPE_CONVERTER_RULE_VARIANT
        TOKEN_SUBTYPE_CONVERTER_RULE_TO
        TOKEN_SUBTYPE_CONVERTER_FLAGS
        TOKEN_SUBTYPE_CONVERTER_FLAG
        TOKEN_SUBTYPE_ATTRIBUTES
        TOKEN_SUBTYPE_ATTR_EQUAL_TMP
        TOKEN_SUBTYPE_ATTR_KEY
        TOKEN_SUBTYPE_ATTR_VALUE
        TOKEN_SUBTYPE_ATOM
        TOKEN_SUBTYPE_HIDDEN
        TOKEN_SUBTYPE_EXT_ATTRS
        TOKEN_SUBTYPE_EXT_INNER
        TOKEN_SUBTYPE_EXT_ATTR_DIRTY
        TOKEN_SUBTYPE_EXT_ATTR
        TOKEN_SUBTYPE_IMAGE_PARAMETER
        TOKEN_SUBTYPE_GALLERY_IMAGE
        TOKEN_SUBTYPE_IMAGEMAP_IMAGE
        TOKEN_SUBTYPE_GALLERY_LINE
        TOKEN_SUBTYPE_GALLERY_PARAM_WRAPPER
        TOKEN_SUBTYPE_IMAGEMAP_LINK_INNER
        TOKEN_SUBTYPE_IMAGEMAP_IMAGE_LINE
        TOKEN_SUBTYPE_IMAGEMAP_LINK
        TOKEN_SUBTYPE_TEMPLATE_NAME
        TOKEN_SUBTYPE_INVOKE_MODULE
        TOKEN_SUBTYPE_INVOKE_FUNCTION
        TOKEN_SUBTYPE_PARAM_LINE
        TOKEN_SUBTYPE_COUNT

    # Map the string view definition explicitly
    ctypedef struct sz_string_view_t:
        const char* start
        size_t length

    # --- Inner Structs of TokenData Union ---
    struct HeadingData:
        int level
    struct CommentData:
        bint closed
    struct HtmlData:
        bint self_closing
        bint closing
        sz_string_view_t orig_tag
    struct TdData:
        sz_string_view_t inner_syntax
    struct DunderData:
        bint case_sensitive
        bint fullwidth
    struct QuoteData:
        bint bold
        bint italic
    struct RedirectData:
        sz_string_view_t pre
        sz_string_view_t post
        sz_string_view_t link
        sz_string_view_t display
    struct ExtData:
        sz_string_view_t name
        sz_string_view_t attr
        sz_string_view_t inner
        sz_string_view_t closing
        bint self_closing
    struct IncludeData:
        sz_string_view_t tag
        sz_string_view_t attr
        sz_string_view_t inner
        sz_string_view_t closing
    struct ExtAttrData:
        sz_string_view_t equal
        char quote_open
        char quote_close
    struct ImageParamData:
        sz_string_view_t raw_syntax
    struct ExtLinkData:
        sz_string_view_t space
    struct LinkData:
        bint magic_pipe
    struct TranscludeData:
        sz_string_view_t modifier

    # --- The TokenData Union ---
    ctypedef union TokenData:
        HeadingData heading
        CommentData comment
        HtmlData html
        TdData td
        DunderData dunder
        QuoteData quote
        RedirectData redirect
        ExtData ext
        IncludeData include_data "include"
        ExtAttrData ext_attr
        ImageParamData image_param
        ExtLinkData ext_link
        LinkData link
        TranscludeData transclude

    # --- Structural Tree Definitions ---
    ctypedef struct Token

    ctypedef struct Child:
        bint is_text
        size_t text_len
        const char* text
        bint text_owned

    ctypedef struct Token:
        TokenType type
        TokenSubType subtype
        char* name
        char sep
        Child* children
        size_t child_count
        size_t child_cap
        TokenData data

    # --- C Library API Functions ---
    void token_free(Token* t) nogil
    char* json_stringify_wikiparser_node(const Token* t, bint pretty) nogil
    char* token_to_string_external(const Token* t) nogil

cdef extern from "wiki_cast/config.h":
    ctypedef struct ParserConfig:
        pass
    ParserConfig* config_load_file(const char* path)
    ParserConfig* config_load_string(const char* json_str, size_t len)
    void config_free(ParserConfig* cfg)

cdef extern from "wiki_cast/parse.h":
    Token* wiki_parse(const char* wikitext, size_t input_len, const ParserConfig* cfg, bint do_include, int max_stage) nogil
    Token* wiki_parse_with_page(const char* wikitext, size_t input_len, const ParserConfig* cfg, bint do_include, int max_stage, const char* page) nogil


cdef class WikiConfig:
    cdef ParserConfig* _cfg

    def __cinit__(self):
        self._cfg = NULL

    @classmethod
    def from_file(cls, str path):
        cdef bytes b_path = path.encode('utf-8')
        cdef WikiConfig instance = WikiConfig.__new__(cls)
        instance._cfg = config_load_file(b_path)
        if instance._cfg == NULL:
            raise RuntimeError(f"Failed to load configuration file at: {path}")
        return instance

    @classmethod
    def from_string(cls, str json_str):
        cdef bytes b_json = json_str.encode('utf-8')
        cdef WikiConfig instance = WikiConfig.__new__(cls)
        instance._cfg = config_load_string(b_json, len(b_json))
        if instance._cfg == NULL:
            raise RuntimeError("Failed to parse configuration JSON string.")
        return instance

    def __dealloc__(self):
        if self._cfg != NULL:
            config_free(self._cfg)


cdef class WikiParser:
    cdef WikiConfig config

    def __init__(self, WikiConfig config):
        if config._cfg == NULL:
            raise ValueError("Provided configuration is uninitialized.")
        self.config = config

    def parse_to_string(self, str wikitext, bint do_include=False, int max_stage=10):
        """
        Parses wikitext directly to its string representation (HTML/Rendered output).
        Gives absolute maximum speed by letting C handle strings entirely.
        """
        if not wikitext:
            return ""

        cdef bytes b_text = wikitext.encode('utf-8')
        
        # --- EXTRACT ALL C TYPES BEFORE NOGIL ---
        cdef const char* c_text = b_text
        cdef size_t text_len = len(b_text)   
        cdef bint c_include = do_include
        cdef int stage = max_stage
        # ----------------------------------------

        cdef Token* root_token = NULL
        cdef char* c_out_str = NULL
        cdef const ParserConfig* cfg = self.config._cfg

        with nogil:
            root_token = wiki_parse(c_text, text_len, cfg, c_include, stage)
            if root_token != NULL:
                c_out_str = token_to_string_external(root_token)

        if root_token == NULL:
            raise RuntimeError("Parsing pipeline allocation failure.")

        try:
            py_string = c_out_str.decode('utf-8') if c_out_str != NULL else ""
        finally:
            if c_out_str != NULL:
                free(c_out_str)
            if root_token != NULL:
                token_free(root_token)

        return py_string

    def parse_to_dict(self, str wikitext, bint do_include=False, int max_stage=10, str page=None):
        """
        Parses wikitext and extracts the structured Abstract Syntax Tree (AST)
        as a native Python dictionary hierarchy.
        """
        if not wikitext:
            return {}

        cdef bytes b_text = wikitext.encode('utf-8')
        
        # --- EXTRACT ALL C TYPES BEFORE NOGIL ---
        cdef const char* c_text = b_text
        cdef size_t text_len = len(b_text)
        cdef bint c_include = do_include
        cdef int stage = max_stage

        cdef bytes b_page = None
        cdef const char* c_page = NULL
        if page is not None:
            b_page = page.encode('utf-8')
            c_page = b_page
        # ----------------------------------------

        cdef Token* root_token = NULL
        cdef char* json_c_str = NULL
        cdef const ParserConfig* cfg = self.config._cfg

        with nogil:
            if c_page != NULL:
                root_token = wiki_parse_with_page(c_text, text_len, cfg, c_include, stage, c_page)
            else:
                root_token = wiki_parse(c_text, text_len, cfg, c_include, stage)
            
            if root_token != NULL:
                json_c_str = json_stringify_wikiparser_node(root_token, False)

        if root_token == NULL:
            raise RuntimeError("Parsing pipeline allocation failure.")

        try:
            if json_c_str == NULL:
                raise RuntimeError("Failed to serialize Token AST to JSON string.")
            py_dict = json.loads(json_c_str.decode('utf-8'))
        finally:
            if json_c_str != NULL:
                free(json_c_str)
            if root_token != NULL:
                token_free(root_token)

        return py_dict

    def parse(self, str wikitext, bint do_include=False, int max_stage=10, str page=None):
        """
        Parses wikitext and returns the raw Abstract Syntax Tree (AST)
        as a PyToken object for direct manipulation.
        """
        if not wikitext:
            return None

        cdef bytes b_text = wikitext.encode('utf-8')
        
        cdef const char* c_text = b_text
        cdef size_t text_len = len(b_text)
        cdef bint c_include = do_include
        cdef int stage = max_stage

        cdef bytes b_page = None
        cdef const char* c_page = NULL
        if page is not None:
            b_page = page.encode('utf-8')
            c_page = b_page

        cdef Token* root_token = NULL
        cdef const ParserConfig* cfg = self.config._cfg

        with nogil:
            if c_page != NULL:
                root_token = wiki_parse_with_page(c_text, text_len, cfg, c_include, stage, c_page)
            else:
                root_token = wiki_parse(c_text, text_len, cfg, c_include, stage)

        if root_token == NULL:
            raise RuntimeError("Parsing pipeline allocation failure.")

        return PyToken.from_ptr(root_token, is_root=True)


# ── Helper for String Views ──────────────────────────────────────────────────
cdef inline str _decode_view(sz_string_view_t view):
    if view.start == NULL or view.length == 0:
        return ""
    return view.start[:view.length].decode('utf-8', errors='replace')


# ── The Unified Wrapper Class ────────────────────────────────────────────────
cdef class PyToken:
    cdef Token* _c_token
    cdef bint _is_root

    @staticmethod
    cdef PyToken from_ptr(Token* t, bint is_root=False):
        if t == NULL:
            return None
        cdef PyToken py_tok = PyToken.__new__(PyToken)
        py_tok._c_token = t
        py_tok._is_root = is_root
        return py_tok

    def __dealloc__(self):
        if self._is_root and self._c_token != NULL:
            with nogil:
                token_free(self._c_token)

    # ── Common Fields ────────────────────────────────────────────────────────
    @property
    def type(self):
        return <int>self._c_token.type if self._c_token != NULL else None

    @property
    def subtype(self):
        return <int>self._c_token.subtype if self._c_token != NULL else None

    @property
    def name(self):
        if self._c_token == NULL or self._c_token.name == NULL:
            return None
        return self._c_token.name.decode('utf-8', errors='replace')

    @property
    def sep(self):
        if self._c_token == NULL or self._c_token.sep == b'\0':
            return ""
        return chr(self._c_token.sep)

    @property
    def children(self):
        if self._c_token == NULL or self._c_token.children == NULL or self._c_token.child_count == 0:
            return []
        
        cdef list out = []
        cdef size_t i
        cdef Child* child = NULL

        for i in range(self._c_token.child_count):
            child = &self._c_token.children[i]
            if child.is_text:
                if child.text != NULL:
                    out.append(child.text[:child.text_len].decode('utf-8', errors='replace'))
            else:
                if child.text != NULL:
                    out.append(PyToken.from_ptr(<Token*>child.text, is_root=False))
        return out

    # ── Union Polymorphic Fields (Guarded by TokenType) ─────────────────────
    
    # TOKEN_HEADING
    @property
    def level(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_HEADING:
            return self._c_token.data.heading.level
        return None

    # TOKEN_COMMENT
    @property
    def comment_closed(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_COMMENT:
            return self._c_token.data.comment.closed
        return None

    # TOKEN_HTML
    @property
    def html_self_closing(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_HTML:
            return self._c_token.data.html.self_closing
        return None

    @property
    def html_closing(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_HTML:
            return self._c_token.data.html.closing
        return None

    @property
    def html_orig_tag(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_HTML:
            return _decode_view(self._c_token.data.html.orig_tag)
        return None

    # TOKEN_TD
    @property
    def td_inner_syntax(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_TD:
            return _decode_view(self._c_token.data.td.inner_syntax)
        return None

    # TOKEN_DOUBLE_UNDERSCORE
    @property
    def dunder_case_sensitive(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_DOUBLE_UNDERSCORE:
            return self._c_token.data.dunder.case_sensitive
        return None

    @property
    def dunder_fullwidth(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_DOUBLE_UNDERSCORE:
            return self._c_token.data.dunder.fullwidth
        return None

    # TOKEN_QUOTE
    @property
    def quote_bold(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_QUOTE:
            return self._c_token.data.quote.bold
        return None

    @property
    def quote_italic(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_QUOTE:
            return self._c_token.data.quote.italic
        return None

    # TOKEN_REDIRECT
    @property
    def redirect_pre(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_REDIRECT:
            return _decode_view(self._c_token.data.redirect.pre)
        return None

    @property
    def redirect_post(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_REDIRECT:
            return _decode_view(self._c_token.data.redirect.post)
        return None

    @property
    def redirect_link(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_REDIRECT:
            return _decode_view(self._c_token.data.redirect.link)
        return None

    @property
    def redirect_display(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_REDIRECT:
            return _decode_view(self._c_token.data.redirect.display)
        return None

    # TOKEN_EXT
    @property
    def ext_name(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_EXT:
            return _decode_view(self._c_token.data.ext.name)
        return None

    @property
    def ext_attr(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_EXT:
            return _decode_view(self._c_token.data.ext.attr)
        return None

    @property
    def ext_inner(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_EXT:
            return _decode_view(self._c_token.data.ext.inner)
        return None

    @property
    def ext_closing(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_EXT:
            return _decode_view(self._c_token.data.ext.closing)
        return None

    @property
    def ext_self_closing(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_EXT:
            return self._c_token.data.ext.self_closing
        return None

    # TOKEN_INCLUDE / TOKEN_NOINCLUDE / TOKEN_ONLYINCLUDE
    @property
    def include_tag(self):
        if self._c_token != NULL and (self._c_token.type == TokenType.TOKEN_INCLUDE or self._c_token.type == TokenType.TOKEN_NOINCLUDE or self._c_token.type == TokenType.TOKEN_ONLYINCLUDE):
            return _decode_view(self._c_token.data.include_data.tag)
        return None

    @property
    def include_attr(self):
        if self._c_token != NULL and (self._c_token.type == TokenType.TOKEN_INCLUDE or self._c_token.type == TokenType.TOKEN_NOINCLUDE or self._c_token.type == TokenType.TOKEN_ONLYINCLUDE):
            return _decode_view(self._c_token.data.include_data.attr)
        return None

    @property
    def include_inner(self):
        if self._c_token != NULL and (self._c_token.type == TokenType.TOKEN_INCLUDE or self._c_token.type == TokenType.TOKEN_NOINCLUDE or self._c_token.type == TokenType.TOKEN_ONLYINCLUDE):
            return _decode_view(self._c_token.data.include_data.inner)
        return None

    @property
    def include_closing(self):
        if self._c_token != NULL and (self._c_token.type == TokenType.TOKEN_INCLUDE or self._c_token.type == TokenType.TOKEN_NOINCLUDE or self._c_token.type == TokenType.TOKEN_ONLYINCLUDE):
            return _decode_view(self._c_token.data.include_data.closing)
        return None

    # TOKEN_EXT_ATTR
    @property
    def ext_attr_equal(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_EXT_ATTR:
            return _decode_view(self._c_token.data.ext_attr.equal)
        return None

    @property
    def ext_attr_quotes(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_EXT_ATTR:
            return (chr(self._c_token.data.ext_attr.quote_open), chr(self._c_token.data.ext_attr.quote_close))
        return None

    # TOKEN_IMAGE_PARAMETER (subtype of TOKEN_EXT)
    @property
    def image_param_raw_syntax(self):
        if self._c_token != NULL and self._c_token.subtype == TokenSubType.TOKEN_SUBTYPE_IMAGE_PARAMETER:
            return _decode_view(self._c_token.data.image_param.raw_syntax)
        return None

    # TOKEN_EXT_LINK
    @property
    def ext_link_space(self):
        if self._c_token != NULL and self._c_token.type == TokenType.TOKEN_EXT_LINK:
            return _decode_view(self._c_token.data.ext_link.space)
        return None
#    return sv.str[:sv.len].decode('utf-8', errors='replace')