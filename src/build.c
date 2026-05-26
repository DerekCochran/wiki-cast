/*
 * build.c — Build phase implementation.
 *
 * Mirrors Token.buildFromStr() and Token.build() in dist/src/index.js:
 *
 *   buildFromStr(str) {
 *     return str.split(/[\0\x7F]/).map((s, i) => {
 *       if (i % 2 === 0) return s && new AstText(s);
 *       const n = Number(s.slice(0, -1));   // strip the type char
 *       return this.#accum[n];
 *     }).filter(Boolean);
 *   }
 *
 *   build() {
 *     const str = this.firstChild.toString();
 *     if (str.includes('\0')) {
 *       setChildNodes(this, 0, 1, this.buildFromStr(str));
 *       if (this.type === 'root') {
 *         for (const token of this.#accum) token?.build();
 *       }
 *     }
 *   }
 */
#include "build.h"
#include "accum.h"
#include "util/log.h"
#include "util/string_util.h"
#include "stringzilla/stringzilla.h"
#include "title.h"
#include "token.h"
#include "util/thread_buffer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *build_normalize_attr_equal(const char *equal, size_t equal_len,
													 Accum *accum) {
	if(!equal || equal_len == 0) return NULL;

	if(memchr(equal, '\0', equal_len) == NULL) {
		char *out= malloc(equal_len + 1);
		if(!out) return NULL;
		memcpy(out, equal, equal_len);
		out[equal_len]= '\0';
		return out;
	}

	Token *tmp= token_new(TOKEN_PLAIN, "attr-equal-tmp");
	if(!tmp) return NULL;
	build_from_str(tmp, equal, equal_len, accum);

	ThreadBuf *scratch= wiki_thread_buf_acquire_scratch();
	if(!scratch) {
		token_free_shallow(tmp);
		return NULL;
	}

	const char *s= token_to_string(tmp, scratch);
	size_t slen= scratch->len;
	char *out= malloc(slen + 1);
	if(out) {
		if(slen > 0 && s) memcpy(out, s, slen);
		out[slen]= '\0';
	}

	wiki_thread_buf_release_scratch(scratch);
	token_free_shallow(tmp);
	return out;
}

static void append_key_token_repr_tb(const Token *t, ThreadBuf *tb) {
	if(!t || !tb) return;

	/* JS toString(true) parity: hidden/include-like tokens serialize as empty. */
	if(t->type == TOKEN_COMMENT || t->type == TOKEN_NOINCLUDE ||
	   t->type == TOKEN_INCLUDE || t->type == TOKEN_DOUBLE_UNDERSCORE) {
		return;
	}

	if(t->type == TOKEN_EXT) {
		const char *ext_tag= t->data.ext.name ? t->data.ext.name : t->name;
		const char *ext_closing= t->data.ext.closing ? t->data.ext.closing : ext_tag;

		wiki_thread_buf_putc(tb, '<');
		if(ext_tag) wiki_thread_buf_append(tb, (sz_string_view_t){ ext_tag, strlen(ext_tag) });
		if(t->child_count > 0) {
			const Child *c= &t->children[0];
			if(c->is_text) {
				wiki_thread_buf_append(tb, (sz_string_view_t){ c->text, c->text_len });
			} else {
				append_key_token_repr_tb(c->token, tb);
			}
		}
		if(t->data.ext.self_closing) {
			wiki_thread_buf_append(tb, (sz_string_view_t){ "/>", 2 });
			return;
		}

		wiki_thread_buf_putc(tb, '>');
		if(t->child_count > 1) {
			const Child *c= &t->children[1];
			if(c->is_text) {
				wiki_thread_buf_append(tb, (sz_string_view_t){ c->text, c->text_len });
			} else {
				append_key_token_repr_tb(c->token, tb);
			}
		}
		wiki_thread_buf_append(tb, (sz_string_view_t){ "</", 2 });
		if(ext_closing) wiki_thread_buf_append(tb, (sz_string_view_t){ ext_closing, strlen(ext_closing) });
		wiki_thread_buf_putc(tb, '>');
		return;
	}

	if(t->type == TOKEN_EXT_ATTR) {
		if(t->child_count > 0) {
			const Child *c= &t->children[0];
			if(c->is_text) {
				wiki_thread_buf_append(tb, (sz_string_view_t){ c->text, c->text_len });
			} else {
				append_key_token_repr_tb(c->token, tb);
			}
		}
		if(t->data.ext_attr.equal) {
			wiki_thread_buf_append(tb, (sz_string_view_t){ t->data.ext_attr.equal, strlen(t->data.ext_attr.equal) });
			if(t->data.ext_attr.quote_open) wiki_thread_buf_putc(tb, t->data.ext_attr.quote_open);
			if(t->child_count > 1) {
				const Child *c= &t->children[1];
				if(c->is_text) {
					wiki_thread_buf_append(tb, (sz_string_view_t){ c->text, c->text_len });
				} else {
					append_key_token_repr_tb(c->token, tb);
				}
			}
			if(t->data.ext_attr.quote_close) wiki_thread_buf_putc(tb, t->data.ext_attr.quote_close);
		}
		return;
	}

	if(t->type == TOKEN_LINK || t->type == TOKEN_FILE ||
	   t->type == TOKEN_CATEGORY || t->type == TOKEN_REDIRECT_TARGET) {
		bool is_file_line_image =
			(t->type == TOKEN_FILE && t->type_name &&
			 (strcmp(t->type_name, "gallery-image") == 0 ||
			  strcmp(t->type_name, "imagemap-image") == 0));

		if(!is_file_line_image) {
			wiki_thread_buf_append(tb, (sz_string_view_t){ "[[", 2 });
		}

		for(size_t i = 0; i < t->child_count; i++) {
			if(i > 0) {
				if(!(i == 1 && t->children[0].is_text && t->children[0].text && t->children[0].text[0] == ':')) {
					if(t->type == TOKEN_FILE) {
						if(t->data.link.magic_pipe)
							wiki_thread_buf_append(tb, (sz_string_view_t){ "{{!}}", 5 });
						else
							wiki_thread_buf_putc(tb, '|');
					} else if(t->data.link.magic_pipe && i == 1) {
						wiki_thread_buf_append(tb, (sz_string_view_t){ "{{!}}", 5 });
					} else {
						wiki_thread_buf_putc(tb, '|');
					}
				}
			}

			const Child *c = &t->children[i];
			if(c->is_text) {
				wiki_thread_buf_append(tb, (sz_string_view_t){ c->text, c->text_len });
			} else {
				append_key_token_repr_tb(c->token, tb);
			}
		}

		if(!is_file_line_image) {
			wiki_thread_buf_append(tb, (sz_string_view_t){ "]]", 2 });
		}
		return;
	}

	if(t->type == TOKEN_HTML) {
		const char *tag= t->data.html.orig_tag ? t->data.html.orig_tag : t->name;
		if(t->data.html.closing) {
			wiki_thread_buf_putc(tb, '<');
			wiki_thread_buf_putc(tb, '/');
			if(tag) wiki_thread_buf_append(tb, (sz_string_view_t){ tag, strlen(tag) });
			if(t->child_count > 0) {
				const Child *c= &t->children[0];
				if(c->is_text) {
					wiki_thread_buf_append(tb, (sz_string_view_t){ c->text, c->text_len });
				} else if(c->token) {
					append_key_token_repr_tb(c->token, tb);
				}
			}
			if(t->data.html.self_closing) {
				wiki_thread_buf_append(tb, (sz_string_view_t){ "/>", 2 });
			} else {
				wiki_thread_buf_putc(tb, '>');
			}
			return;
		}

		wiki_thread_buf_putc(tb, '<');
		if(tag) wiki_thread_buf_append(tb, (sz_string_view_t){ tag, strlen(tag) });
		if(t->child_count > 0) {
			const Child *c= &t->children[0];
			if(c->is_text) {
				wiki_thread_buf_append(tb, (sz_string_view_t){ c->text, c->text_len });
			} else if(c->token) {
				append_key_token_repr_tb(c->token, tb);
			}
		}
		if(t->data.html.self_closing) {
			wiki_thread_buf_append(tb, (sz_string_view_t){ "/>", 2 });
		} else {
			wiki_thread_buf_putc(tb, '>');
		}
		return;
	}

	if(t->type == TOKEN_EXT_LINK) {
		wiki_thread_buf_putc(tb, '[');
		if(t->child_count > 0) {
			const Child *c0= &t->children[0];
			if(c0->is_text) {
				wiki_thread_buf_append(tb, (sz_string_view_t){ c0->text, c0->text_len });
			} else {
				append_key_token_repr_tb(c0->token, tb);
			}

			if(t->child_count == 1) {
				if(t->data.ext_link.space) {
					wiki_thread_buf_append(tb, (sz_string_view_t){ t->data.ext_link.space, strlen(t->data.ext_link.space) });
				}
			} else {
				if(t->data.ext_link.space) {
					wiki_thread_buf_append(tb, (sz_string_view_t){ t->data.ext_link.space, strlen(t->data.ext_link.space) });
				} else {
					wiki_thread_buf_putc(tb, ' ');
				}
				for(size_t i= 1; i < t->child_count; i++) {
					const Child *ci= &t->children[i];
					if(ci->is_text) {
						wiki_thread_buf_append(tb, (sz_string_view_t){ ci->text, ci->text_len });
					} else {
						append_key_token_repr_tb(ci->token, tb);
					}
				}
			}
		}
		wiki_thread_buf_putc(tb, ']');
		return;
	}

	if(t->type == TOKEN_TRANSCLUDE) {
		wiki_thread_buf_putc(tb, '{');
		wiki_thread_buf_putc(tb, '{');
		bool is_magic_word = (t->type_name && strcmp(t->type_name, "magic-word") == 0);
		for(size_t i = 0; i < t->child_count; i++) {
			if(i > 0) {
				if(is_magic_word && i == 1)
					wiki_thread_buf_putc(tb, ':');
				else
					wiki_thread_buf_putc(tb, '|');
			}
			const Child *c = &t->children[i];
			if(c->is_text) {
				sz_string_view_t v = { c->text, c->text_len };
				wiki_thread_buf_append(tb, v);
			} else {
				append_key_token_repr_tb(c->token, tb);
			}
		}
		wiki_thread_buf_putc(tb, '}');
		wiki_thread_buf_putc(tb, '}');
		return;
	}

	if(t->type == TOKEN_PARAMETER && t->child_count >= 2) {
		const Child *k = &t->children[0];
		const Child *v = &t->children[1];
		bool anon = false;
		if(k->is_text) anon = k->text_len == 0;
		else if(k->token) anon = k->token->child_count == 0;
		if(!anon) {
			if(k->is_text) {
				sz_string_view_t vk = { k->text, k->text_len };
				wiki_thread_buf_append(tb, vk);
			} else {
				append_key_token_repr_tb(k->token, tb);
			}
			wiki_thread_buf_putc(tb, '=');
		}
		if(v->is_text) {
			sz_string_view_t vv = { v->text, v->text_len };
			wiki_thread_buf_append(tb, vv);
		} else {
			append_key_token_repr_tb(v->token, tb);
		}
		return;
	}

	if(t->type == TOKEN_ARG) {
		/* JS parity for Token.text() on ArgToken: preserve {{{...}}} syntax
		 * when an arg token appears inside a template-name token. */
		wiki_thread_buf_putc(tb, '{');
		wiki_thread_buf_putc(tb, '{');
		wiki_thread_buf_putc(tb, '{');
		for(size_t i = 0; i < t->child_count; i++) {
			if(i > 0) wiki_thread_buf_putc(tb, '|');
			const Child *c = &t->children[i];
			if(c->is_text) {
				sz_string_view_t v = { c->text, c->text_len };
				wiki_thread_buf_append(tb, v);
			} else {
				append_key_token_repr_tb(c->token, tb);
			}
		}
		wiki_thread_buf_putc(tb, '}');
		wiki_thread_buf_putc(tb, '}');
		wiki_thread_buf_putc(tb, '}');
		return;
	}

	for(size_t i = 0; i < t->child_count; i++) {
		if(i > 0 && t->sep != '\0') {
			wiki_thread_buf_putc(tb, t->sep);
		}
		const Child *c = &t->children[i];
		if(c->is_text) {
			sz_string_view_t v = { c->text, c->text_len };
			wiki_thread_buf_append(tb, v);
		} else {
			TokenType tt = c->token ? c->token->type : TOKEN_TEXT;
			if(tt == TOKEN_COMMENT || tt == TOKEN_NOINCLUDE ||
			   tt == TOKEN_INCLUDE || tt == TOKEN_DOUBLE_UNDERSCORE) continue;
			append_key_token_repr_tb(c->token, tb);
		}
	}
}

/* JS String.prototype.trim() parity for template-name normalization. */
static size_t js_trim_ws_at(const char *s, size_t len, size_t i) {
	if(i >= len) return 0;
	unsigned char c0= (unsigned char)s[i];
	if(c0 == 0x09 || c0 == 0x0A || c0 == 0x0B || c0 == 0x0C || c0 == 0x0D || c0 == 0x20) return 1;
	if(i + 1 < len && c0 == 0xC2 && (unsigned char)s[i + 1] == 0xA0) return 2; /* U+00A0 */
	if(i + 2 < len && c0 == 0xE1 && (unsigned char)s[i + 1] == 0x9A && (unsigned char)s[i + 2] == 0x80) return 3; /* U+1680 */
	if(i + 2 < len && c0 == 0xE2 && (unsigned char)s[i + 1] == 0x80) {
		unsigned char c2= (unsigned char)s[i + 2];
		if((c2 >= 0x80 && c2 <= 0x8A) || c2 == 0xA8 || c2 == 0xA9 || c2 == 0xAF) return 3; /* U+2000..U+200A/U+2028/U+2029/U+202F */
	}
	if(i + 2 < len && c0 == 0xE2 && (unsigned char)s[i + 1] == 0x81 && (unsigned char)s[i + 2] == 0x9F) return 3; /* U+205F */
	if(i + 2 < len && c0 == 0xE3 && (unsigned char)s[i + 1] == 0x80 && (unsigned char)s[i + 2] == 0x80) return 3; /* U+3000 */
	if(i + 2 < len && c0 == 0xEF && (unsigned char)s[i + 1] == 0xBB && (unsigned char)s[i + 2] == 0xBF) return 3; /* U+FEFF */
	return 0;
}

static size_t js_trim_ws_before(const char *s, size_t end) {
	if(end == 0) return 0;
	unsigned char c1= (unsigned char)s[end - 1];
	if(c1 == 0x09 || c1 == 0x0A || c1 == 0x0B || c1 == 0x0C || c1 == 0x0D || c1 == 0x20) return 1;
	if(end >= 2 && (unsigned char)s[end - 2] == 0xC2 && (unsigned char)s[end - 1] == 0xA0) return 2;
	if(end >= 3) {
		unsigned char c0= (unsigned char)s[end - 3];
		unsigned char c2= (unsigned char)s[end - 2];
		unsigned char c3= (unsigned char)s[end - 1];
		if(c0 == 0xE1 && c2 == 0x9A && c3 == 0x80) return 3;
		if(c0 == 0xE2 && c2 == 0x80 && ((c3 >= 0x80 && c3 <= 0x8A) || c3 == 0xA8 || c3 == 0xA9 || c3 == 0xAF)) return 3;
		if(c0 == 0xE2 && c2 == 0x81 && c3 == 0x9F) return 3;
		if(c0 == 0xE3 && c2 == 0x80 && c3 == 0x80) return 3;
		if(c0 == 0xEF && c2 == 0xBB && c3 == 0xBF) return 3;
	}
	return 0;
}

/* JS parity: TranscludeToken.afterBuild() sets the normalized template name.
 * In JS this happens after build() completes, so the name is absent from
 * stage-log snapshots captured during parseBraces (Stage 1). */
static void refresh_template_name(Token *t, const ParserConfig *cfg) {
	if(!t || t->type != TOKEN_TRANSCLUDE) return;
	if(!t->type_name || strcmp(t->type_name, "template") != 0) return;
	if(t->child_count == 0) return;

	/* Child 0 is the template-name atom token */
	const Child *c= &t->children[0];
	if(c->is_text || !c->token) return;

	/* Concatenate the text content of the template-name token into a scratch buffer */
	ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();
	append_key_token_repr_tb(c->token, scratch);
	size_t len = scratch->len;
	if(len == 0) {
		wiki_thread_buf_release_scratch(scratch);
		return;
	}

	/* JS parity: trimLc() uses String.trim(), including Unicode whitespace. */
	char *text = scratch->buf;
	size_t start= 0;
	size_t end= len;
	while(start < end) {
		size_t ws= js_trim_ws_at(text, end, start);
		if(ws == 0) break;
		start+= ws;
	}
	while(end > start) {
		size_t ws= js_trim_ws_before(text, end);
		if(ws == 0) break;
		end-= ws;
	}
	if(start > 0) {
		memmove(text, text + start, end - start);
	}
	len= end - start;
	text[len]= '\0';
	if(len == 0) {
		wiki_thread_buf_release_scratch(scratch);
		return;
	}

	Title *parsed= title_parse_half_parsed(text, len, 10, cfg, true, NULL);
	/* release scratch now that parsed has copied any needed data */
	wiki_thread_buf_release_scratch(scratch);
	if(!parsed || !parsed->title || !parsed->title[0]) {
		title_free(parsed);
		return;
	}
	char *name= strdup(parsed->title);
	title_free(parsed);

	if(name) {
		free(t->name);
		t->name= name;
	}
}

static void refresh_attribute_name(Token *t) {
	if(!t || t->type != TOKEN_EXT_ATTR || t->child_count == 0) return;

	Child *key= &t->children[0];
	char *new_name= NULL;

	if(key->is_text) {
		new_name= str_trim_lc(key->text, key->text_len);
	} else if(key->token) {
		ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();
		append_key_token_repr_tb(key->token, scratch);
		new_name = str_trim_lc(scratch->buf, scratch->len);
		wiki_thread_buf_release_scratch(scratch);
	}

	if(new_name) {
		free(t->name);
		t->name= new_name;
	}
}

/* JS parity: ParameterToken.afterBuild() recomputes named-parameter keys from
 * the built parameter-key token (so embedded tokens like {{LASTYEAR}} are
 * reflected in param->name). Anonymous parameters keep their numeric names. */
static void refresh_parameter_name(Token *t) {
	if(!t || t->type != TOKEN_PARAMETER || t->child_count == 0) return;

	Child *key= &t->children[0];
	ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();
	if(!scratch) return;

	if(key->is_text) {
		sz_string_view_t vk = { key->text, key->text_len };
		wiki_thread_buf_append(scratch, vk);
	} else if(key->token) {
		/* JS parity: ParameterToken.trimName() uses keyToken.toString(true). */
		append_key_token_repr_tb(key->token, scratch);
	}

	/* JS trimName regex trims only [ \t\n\0\v] at both ends. */
	size_t i= 0;
	size_t j= scratch->len;
	const char *buf= scratch->buf;
	while(i < j && (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\n' || buf[i] == '\0' || buf[i] == '\v')) i++;
	while(j > i && (buf[j - 1] == ' ' || buf[j - 1] == '\t' || buf[j - 1] == '\n' || buf[j - 1] == '\0' || buf[j - 1] == '\v')) j--;

	/* Empty key means anonymous parameter; keep existing numeric name. */
	if(j == i) {
		wiki_thread_buf_release_scratch(scratch);
		return;
	}

	size_t n= j - i;
	char *new_name= malloc(n + 1);
	if(new_name) {
		sz_copy(new_name, buf + i, n);
		new_name[n]= '\0';
		free(t->name);
		t->name= new_name;
	}

	wiki_thread_buf_release_scratch(scratch);
}

void build_from_str(Token *parent, const char *str, size_t str_len,
										Accum *accum) {
	/* Operate directly on the provided buffer `str` (binary-safe, may contain NULs).
	 * This avoids an extra malloc+memcpy for large buffers while preserving
	 * identical behavior: all reads use explicit lengths rather than relying
	 * on NUL-termination. */
	const char *s= str;

	/* If caller passed one of our owned child buffers as input, copy it first
	 * because existing children are freed before reconstruction. */
	char *src_copy= NULL;
	for(size_t i= 0; i < parent->child_count; i++) {
		Child *c= &parent->children[i];
		if(c->is_text && c->text == str) {
			if(c->text_owned) {
				src_copy= malloc(str_len + 1);
				if(!src_copy) {
					log_fatal("build_from_str: malloc failed while copying aliased input");
					abort();
				}
				if(str_len > 0) memcpy(src_copy, str, str_len);
				src_copy[str_len]= '\0';
				s= src_copy;
			}
		}
	}

	/* Free existing children first. Token children are owned by the accum. */
	for(size_t i= 0; i < parent->child_count; i++) {
		Child *c= &parent->children[i];
		if(c->is_text && c->text_owned && c->text) {
			free((void *)c->text);
		}
	}

	parent->child_count= 0;

	/* Walk str, splitting on \0 … \x7F markers.
     * The marker format is: \0 <decimal digits> <type_char> \x7F
     * We split at \0 and \x7F boundaries, alternating text / reference.
     * Bit-by-bit:
     *   Even segments (before first \0, between .\x7F and next \0): text nodes
     *   Odd segments (between \0 and \x7F, i.e. "N<ch>"): token reference
     */
	size_t seg_start= 0;
	bool in_marker= false;

	for(size_t i= 0; i <= str_len;) {
		unsigned char c= (i < str_len) ? (unsigned char)s[i] : 0;

		if(!in_marker) {
			if(c == '\0' || i == str_len) {
				/* Emit text segment [seg_start, i) */
				size_t text_len= i - seg_start;
				if(text_len > 0) {
					const char *p = wiki_thread_buf_append_to_tokens(s + seg_start, text_len);
					token_append_text_n(parent, p, text_len);
				}
				if(c == '\0') {
					seg_start= i + 1;
					in_marker= true;
				}
				i++;
			} else {
				i++;
			}
		} else {
			/* Inside marker — find the \x7F */
			if(c == '\x7F' || i == str_len) {
				/* Segment is "N<type_ch>" where N is decimal */
				const char *marker_content= s + seg_start;
				size_t marker_len= i - seg_start;
				bool missing_terminator= (c != '\x7F');

				if(missing_terminator || marker_len < 2) {
					fprintf(stderr,
					        "DEBUG build_from_str: INVALID sentinel at parent=%p, reason=%s, marker_len=%zu, seg_start=%zu, cursor=%zu\n",
					        (void*)parent,
					        missing_terminator ? "missing DEL terminator" : "marker too short",
					        marker_len,
					        seg_start,
					        i);
					fprintf(stderr, "DEBUG build_from_str: INVALID sentinel raw bytes: ");
					for(size_t dbg= 0; dbg < marker_len; dbg++) {
						unsigned char ch= (unsigned char)marker_content[dbg];
						fprintf(stderr, "[%zu]=0x%02x '%c' ", dbg, ch,
						        (ch >= 0x20 && ch < 0x7f) ? ch : '?');
					}
					fprintf(stderr, "\n");
					log_fatal("build_from_str: invalid sentinel (reason=%s, marker_len=%zu, seg_start=%zu, cursor=%zu)",
					          missing_terminator ? "missing DEL terminator" : "marker too short",
					          marker_len,
					          seg_start,
					          i);
					abort();
				}

				/* Parse decimal index (all but last char). */
				size_t idx= 0;
				for(size_t d= 0; d < marker_len - 1; d++) {
					unsigned char dc= (unsigned char)marker_content[d];
					if(dc < '0' || dc > '9') {
						fprintf(stderr,
						        "DEBUG build_from_str: INVALID sentinel at parent=%p, marker_len=%zu, failed_at=%zu, byte=0x%02x\n",
						        (void*)parent,
						        marker_len,
						        d,
						        dc);
						fprintf(stderr, "DEBUG build_from_str: INVALID sentinel raw bytes: ");
						for(size_t dbg= 0; dbg < marker_len; dbg++) {
							unsigned char ch= (unsigned char)marker_content[dbg];
							fprintf(stderr, "[%zu]=0x%02x '%c' ", dbg, ch,
							        (ch >= 0x20 && ch < 0x7f) ? ch : '?');
						}
						fprintf(stderr, "\n");
						log_fatal("build_from_str: invalid sentinel digit at offset=%zu (byte=0x%02x)", d, dc);
						abort();
					}
					idx= idx * 10 + (size_t)(dc - '0');
				}

				Token *child= accum_get(accum, idx);
				if(!child) {
					unsigned char type_ch= (unsigned char)marker_content[marker_len - 1];
					fprintf(stderr,
					        "DEBUG build_from_str: INVALID sentinel unresolved idx=%zu type=0x%02x '%c' marker_len=%zu\n",
					        idx,
					        type_ch,
					        (type_ch >= 0x20 && type_ch < 0x7f) ? type_ch : '?',
					        marker_len);
					fprintf(stderr, "DEBUG build_from_str: INVALID sentinel raw bytes: ");
					for(size_t dbg= 0; dbg < marker_len; dbg++) {
						unsigned char ch= (unsigned char)marker_content[dbg];
						fprintf(stderr, "[%zu]=0x%02x '%c' ", dbg, ch,
						        (ch >= 0x20 && ch < 0x7f) ? ch : '?');
					}
					fprintf(stderr, "\n");
					log_fatal("build_from_str: sentinel points to missing accum index=%zu", idx);
					abort();
				}
				token_append_child(parent, child);

				seg_start= i + 1;
				in_marker= false;
				i++;
			} else {
				i++;
			}
		}
	}

	if(src_copy) free(src_copy);
}

/* Recursively expand sentinel markers in all text descendants of a token.
 * Mirrors JS Token.build() which calls buildFromStr on each token's firstChild
 * text when it contains a \0 sentinel. */
void build_token_recursive(Token *t, Accum *accum,
												 const ParserConfig *cfg) {
	if(!t) return;

	bool has_marker_text= false;
	bool all_text_children= true;
	size_t total_text_len= 0;

	for(size_t j= 0; j < t->child_count; j++) {
		Child *c= &t->children[j];
		if(!c->is_text) {
			all_text_children= false;
			continue;
		}
		total_text_len+= c->text_len;
		if(c->text) {
			char needle = '\x7F';
			if(sz_find_byte(c->text, c->text_len, &needle)) {
				has_marker_text = true;
			}
		}
	}

	if(has_marker_text && all_text_children) {
		ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();
		/* Pre-reserve the combined length to avoid repeated growth */
		wiki_thread_buf_reserve(scratch, total_text_len);
		for(size_t j= 0; j < t->child_count; j++) {
			Child *c= &t->children[j];
			sz_string_view_t v = { c->text, c->text_len };
			wiki_thread_buf_append(scratch, v);
		}
		build_from_str(t, scratch->buf, scratch->len, accum);
		wiki_thread_buf_release_scratch(scratch);
	}

	/* For each child of t: */
	for(size_t j= 0; j < t->child_count; j++) {
		Child *c= &t->children[j];
			if(c->is_text) {
			const char *text= c->text;
			size_t text_len= c->text_len;
			if(text) {
				char needle = '\x7F';
				if(sz_find_byte(text, text_len, &needle)) {
					/* This token (t) has a text child with sentinels.
					 * Rebuild t from this text, which replaces all children.
					 * Note: build_from_str frees all children (including this
					 * text child) and replaces them. After this call, child
					 * positions shift, so we exit the loop. */
					build_from_str(t, text, text_len, accum);
					break;
				}
			}
		} else if(c->token) {
			/* Recurse into child token */
			build_token_recursive(c->token, accum, cfg);
		}
	}

	/* JS AttributeToken.afterBuild parity: recompute name from attr-key text,
     * so keys that were sentinel-expanded (for example {{green}}) get the
     * correct final attribute name. */
	refresh_attribute_name(t);

	/* JS ParameterToken.afterBuild parity: update parameter name from the
	 * expanded parameter-key token after build recursion completes. */
	refresh_parameter_name(t);

	/* JS TranscludeToken.afterBuild parity: template name is set after build,
     * not during parseBraces, so it is absent from stage-log snapshots. */
	refresh_template_name(t, cfg);
}

/* Get the syntax last-character and independence flag from a TOKEN_TD.
 * "Independence" means the syntax starts with \n (a new-line cell, not inline). */
static char td_syntax_last_char(const Token *td, bool *is_independent) {
	if(is_independent) *is_independent= false;
	if(!td || td->child_count == 0) return '|';
	const Child *sc0= &td->children[0]; /* syntax child */
	const Token *syn= sc0->is_text ? NULL : sc0->token;
	if(!syn || syn->child_count == 0) return '|';
	const Child *stc= &syn->children[0]; /* text child of syntax */
	if(!stc->is_text || !stc->text || stc->text_len == 0) return '|';
	if(is_independent) *is_independent= (stc->text[0] == '\n');
	return stc->text[stc->text_len - 1];
}

static void set_td_attrs_name(Token *td, const char *name) {
	if(!td || td->child_count < 2) return;
	Child *ac= &td->children[1];
	if(ac->is_text || !ac->token || ac->token->type != TOKEN_ATTRIBUTES) return;
	char *dup = NULL;
	if(name) {
		size_t nlen = strlen(name);
		dup = malloc(nlen + 1);
		if(dup) {
			memcpy(dup, name, nlen);
			dup[nlen] = '\0';
		}
	}
	free(ac->token->name);
	ac->token->name = dup;
}

/* JS parity: AttributesToken.afterBuild() calls parentNode.subtype for 'td'
 * tokens, where subtype is computed by TdToken.#getSyntax() which implements
 * sibling-inheritance: a non-independent cell (||/!!) inherits the subtype of
 * its previous sibling.  This function applies that logic to all TD children
 * of a container (TABLE/TR/ROOT/etc.) and recurses into children. */
void propagate_table_subtypes(Token *t) {
	if(!t) return;

	/* Process this token's TD children with sibling inheritance */
	const char *running= "td";
	for(size_t k= 0; k < t->child_count; k++) {
		const Child *c= &t->children[k];
		if(c->is_text || !c->token || c->token->type != TOKEN_TD) continue;
		Token *td= c->token;

		bool independent;
		char last= td_syntax_last_char(td, &independent);
		const char *own_subtype=
			(last == '!') ? "th" : (last == '+') ? "caption" : "td";

		if(independent) {
			/* Independent cell: own syntax determines subtype; reset running */
			running= own_subtype;
		} else {
			/* Non-independent (||/!!): inherit running unless own type forces 'th' */
			if(own_subtype[0] == 't' && own_subtype[1] == 'h') running= "th";
			/* else: running stays (inherit) */
		}
		set_td_attrs_name(td, running);
	}

	/* Recurse into all non-text children (including TR, TABLE, etc.) */
	for(size_t k= 0; k < t->child_count; k++) {
		const Child *c= &t->children[k];
		if(!c->is_text && c->token) {
			propagate_table_subtypes(c->token);
		}
	}
}

void build(Token *root, const ThreadBuf *tb, Accum *accum,
				 const ParserConfig *cfg) {
	/* Step 1: Expand the root's working string (which has embedded \0 sentinels). */
	build_from_str(root, tb->buf, tb->len, accum);

	/* Step 2: Recursively build any sub-tokens whose text children contain
     * sentinel markers (e.g. ext-inner content from later stages, or nested
     * templates whose parameter-value contains a sentinel for an inner token). */
	for(size_t i= 0; i < accum->count; i++) {
		Token *t= accum->tokens[i];
		if(!t || t == root) continue;
		build_token_recursive(t, accum, cfg);
	}

	/* Step 3: JS AttributesToken.afterBuild() parity — propagate TD subtype
     * ('th'/'td'/'caption') to each table-attrs token. This must run AFTER
     * the full accum loop so that syntax tokens are fully expanded and sibling
     * context is stable. */
	propagate_table_subtypes(root);
}
