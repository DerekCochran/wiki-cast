# cython: language_level=3
import json
from libc.stdlib cimport free

# 1. Map the C Enums so they are exposed natively to Python
cpdef enum PythonTokenType:
    TOKEN_TEXT = 0
    TOKEN_ROOT = 1
    TOKEN_PLAIN = 2
    TOKEN_COMMENT = 3
    TOKEN_EXT = 4
    TOKEN_NOINCLUDE = 5
    TOKEN_INCLUDE = 6
    TOKEN_TRANSLATE = 7
    TOKEN_ONLYINCLUDE = 8
    TOKEN_ARG = 9
    TOKEN_TRANSCLUDE = 10
    TOKEN_HEADING = 11
    TOKEN_HTML = 12
    TOKEN_TABLE = 13
    TOKEN_TR = 14
    TOKEN_TD = 15
    TOKEN_HR = 16
    TOKEN_DOUBLE_UNDERSCORE = 17
    TOKEN_LINK = 18
    TOKEN_FILE = 19
    TOKEN_CATEGORY = 20
    TOKEN_REDIRECT = 21
    TOKEN_QUOTE = 24
    TOKEN_EXT_LINK = 25

cpdef enum PythonTokenSubType:
    SUBTYPE_NONE = 0
    SUBTYPE_ROOT = 1
    SUBTYPE_REDIRECT = 2
    SUBTYPE_REDIRECT_SYNTAX = 3
    SUBTYPE_REDIRECT_TARGET = 4
    SUBTYPE_COMMENT = 5
    SUBTYPE_EXT = 6
    SUBTYPE_NOINCLUDE = 7
    SUBTYPE_INCLUDE = 8
    SUBTYPE_INCLUDEONLY = 9
    SUBTYPE_ONLYINCLUDE = 10
    SUBTYPE_TRANSLATE = 11
    SUBTYPE_ARG = 12
    SUBTYPE_ARG_NAME = 13
    SUBTYPE_ARG_DEFAULT = 14
    SUBTYPE_TEMPLATE = 15
    SUBTYPE_MAGIC_WORD = 16
    SUBTYPE_MAGIC_WORD_NAME = 17
    SUBTYPE_PARAMETER = 18
    SUBTYPE_PARAMETER_KEY = 19
    SUBTYPE_PARAMETER_VALUE = 20
    SUBTYPE_HEADING = 21
    SUBTYPE_HEADING_TITLE = 22
    SUBTYPE_HEADING_TRAIL = 23
    SUBTYPE_HTML = 24
    SUBTYPE_HTML_ATTRS = 25
    SUBTYPE_HTML_ATTR = 26
    SUBTYPE_HTML_ATTR_DIRTY = 27
    SUBTYPE_TABLE = 28
    SUBTYPE_TR = 29
    SUBTYPE_TD = 30
    SUBTYPE_TABLE_SYNTAX = 31
    SUBTYPE_TABLE_ATTRS = 32
    SUBTYPE_TABLE_ATTR = 33
    SUBTYPE_TABLE_ATTR_DIRTY = 34
    SUBTYPE_TABLE_INTER = 35
    SUBTYPE_TABLE_INNER = 36
    SUBTYPE_TD_INNER = 37
    SUBTYPE_HR = 38
    SUBTYPE_DOUBLE_UNDERSCORE = 39
    SUBTYPE_LINK = 40
    SUBTYPE_FILE = 41
    SUBTYPE_CATEGORY = 42
    SUBTYPE_TEXT = 43
    SUBTYPE_LINK_TARGET = 44
    SUBTYPE_LINK_TEXT = 45
    SUBTYPE_QUOTE = 46
    SUBTYPE_EXT_LINK = 47
    SUBTYPE_EXT_LINK_URL = 48
    SUBTYPE_EXT_LINK_TEXT = 49
    SUBTYPE_MAGIC_LINK = 50
    SUBTYPE_FREE_EXT_LINK = 51
    SUBTYPE_LIST = 52
    SUBTYPE_DD = 53
    SUBTYPE_CONVERTER = 54
    SUBTYPE_CONVERTER_RULE = 55
    SUBTYPE_CONVERTER_RULE_FROM = 56
    SUBTYPE_CONVERTER_RULE_VARIANT = 57
    SUBTYPE_CONVERTER_RULE_TO = 58
    SUBTYPE_CONVERTER_FLAGS = 59
    SUBTYPE_CONVERTER_FLAG = 60
    SUBTYPE_ATTRIBUTES = 61
    SUBTYPE_ATTR_EQUAL_TMP = 62
    SUBTYPE_ATTR_KEY = 63
    SUBTYPE_ATTR_VALUE = 64
    SUBTYPE_ATOM = 65
    SUBTYPE_HIDDEN = 66
    SUBTYPE_EXT_ATTRS = 67
    SUBTYPE_EXT_INNER = 68
    SUBTYPE_EXT_ATTR_DIRTY = 69
    SUBTYPE_EXT_ATTR = 70
    SUBTYPE_IMAGE_PARAMETER = 71
    SUBTYPE_GALLERY_IMAGE = 72
    SUBTYPE_IMAGEMAP_IMAGE = 73
    SUBTYPE_GALLERY_LINE = 74
    SUBTYPE_GALLERY_PARAM_WRAPPER = 75
    SUBTYPE_IMAGEMAP_LINK_INNER = 76
    SUBTYPE_IMAGEMAP_IMAGE_LINE = 77
    SUBTYPE_IMAGEMAP_LINK = 78
    SUBTYPE_TEMPLATE_NAME = 79
    SUBTYPE_INVOKE_MODULE = 80
    SUBTYPE_INVOKE_FUNCTION = 81
    SUBTYPE_PARAM_LINE = 82

# 2. Forward-declare the exact C structures from token.h and parse.h
cdef extern from "wiki_cast/token.h":
    struct Token:
        int type
        int subtype
        char* name
        size_t child_count
    void token_free(Token* t)
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