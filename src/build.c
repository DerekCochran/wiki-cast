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
#include "log.h"
#include "string_util.h"
#include "title.h"
#include "token.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sb_reserve(char **buf, size_t *cap, size_t need) {
	while(*cap < need) {
		*cap*= 2;
		*buf= realloc(*buf, *cap);
		assert(*buf);
	}
}

static void sb_append(char **buf, size_t *len, size_t *cap, const char *s, size_t n) {
	sb_reserve(buf, cap, *len + n + 1);
	memcpy(*buf + *len, s, n);
	*len+= n;
	(*buf)[*len]= '\0';
}

static void append_key_token_repr(const Token *t, char **buf, size_t *len, size_t *cap) {
	if(!t) return;

	if(t->type == TOKEN_TRANSCLUDE) {
		sb_append(buf, len, cap, "{{", 2);
		bool is_magic_word= (t->type_name && strcmp(t->type_name, "magic-word") == 0);
		for(size_t i= 0; i < t->child_count; i++) {
			if(i > 0) {
				if(is_magic_word && i == 1)
					sb_append(buf, len, cap, ":", 1);
				else
					sb_append(buf, len, cap, "|", 1);
			}
			const Child *c= &t->children[i];
			if(c->is_text) {
				sb_append(buf, len, cap, c->text, c->text_len);
			} else {
				append_key_token_repr(c->token, buf, len, cap);
			}
		}
		sb_append(buf, len, cap, "}}", 2);
		return;
	}

	if(t->type == TOKEN_PARAMETER && t->child_count >= 2) {
		const Child *k= &t->children[0];
		const Child *v= &t->children[1];
		bool anon= false;
		if(k->is_text) {
			anon= k->text_len == 0;
		} else if(k->token) {
			anon= k->token->child_count == 0;
		}
		if(!anon) {
			if(k->is_text)
				sb_append(buf, len, cap, k->text, k->text_len);
			else
				append_key_token_repr(k->token, buf, len, cap);
			sb_append(buf, len, cap, "=", 1);
		}
		if(v->is_text)
			sb_append(buf, len, cap, v->text, v->text_len);
		else
			append_key_token_repr(v->token, buf, len, cap);
		return;
	}

	for(size_t i= 0; i < t->child_count; i++) {
		if(i > 0 && t->sep != '\0') {
			sb_append(buf, len, cap, &t->sep, 1);
		}
		const Child *c= &t->children[i];
		if(c->is_text) {
			sb_append(buf, len, cap, c->text, c->text_len);
		} else {
			/* JS parity: removeComment() strips sentinels [cn] before normalizeTitle.
			 * After build(), those sentinels are expanded to real token children, so
			 * we skip comment/noinclude/double-underscore tokens here to match JS. */
			TokenType tt= c->token ? c->token->type : TOKEN_TEXT;
			if(tt == TOKEN_COMMENT || tt == TOKEN_NOINCLUDE ||
			   tt == TOKEN_INCLUDE || tt == TOKEN_DOUBLE_UNDERSCORE) continue;
			append_key_token_repr(c->token, buf, len, cap);
		}
	}
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

	/* Concatenate the text content of the template-name token */
	size_t cap= 64, len= 0;
	char *text= malloc(cap);
	assert(text);
	text[0]= '\0';
	append_key_token_repr(c->token, &text, &len, &cap);
	if(len == 0) {
		free(text);
		return;
	}

	/* JS parity: trimLc() is applied before normalizeTitle, so strip all
     * leading/trailing whitespace (including \n) from the raw name text. */
	while(len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t' ||
										text[len - 1] == '\n' || text[len - 1] == '\r' ||
										text[len - 1] == '\f' || text[len - 1] == '\v')) len--;
	size_t skip= 0;
	while(skip < len && (text[skip] == ' ' || text[skip] == '\t' ||
											 text[skip] == '\n' || text[skip] == '\r' ||
											 text[skip] == '\f' || text[skip] == '\v')) skip++;
	if(skip) {
		memmove(text, text + skip, len - skip);
		len-= skip;
	}
	text[len]= '\0';
	if(len == 0) {
		free(text);
		return;
	}

	Title *parsed= title_parse_half_parsed(text, len, 10, cfg, true, "");
	free(text);
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
		size_t cap= 64, len= 0;
		char *tmp= malloc(cap);
		assert(tmp);
		tmp[0]= '\0';
		append_key_token_repr(key->token, &tmp, &len, &cap);
		new_name= str_trim_lc(tmp, len);
		free(tmp);
	}

	if(new_name) {
		free(t->name);
		t->name= new_name;
	}
}

void build_from_str(Token *parent, const char *str, size_t str_len,
										Accum *accum) {
	char *src= malloc(str_len + 1);
	assert(src);
	memcpy(src, str, str_len);
	src[str_len]= '\0';

	/* Free existing children first */
	for(size_t i= 0; i < parent->child_count; i++) {
		Child *c= &parent->children[i];
		if(c->is_text) free(c->text);
		/* Token pointers are owned by the accum — do NOT free them here */
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
		unsigned char c= (i < str_len) ? (unsigned char)src[i] : 0;

		if(!in_marker) {
			if(c == '\0' || i == str_len) {
				/* Emit text segment [seg_start, i) */
				size_t text_len= i - seg_start;
				if(text_len > 0) {
					token_append_text_n(parent, src + seg_start, text_len);
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
				const char *marker_content= src + seg_start;
				size_t marker_len= i - seg_start;

				if(marker_len >= 2) {
					/* Parse decimal index (all but last char) */
					size_t idx= 0;
					bool valid= true;
					for(size_t d= 0; d < marker_len - 1; d++) {
						char dc= marker_content[d];
						if(dc >= '0' && dc <= '9') {
							idx= idx * 10 + (size_t)(dc - '0');
						} else {
							valid= false;
							break;
						}
					}
					if(valid) {
						Token *child= accum_get(accum, idx);
						if(child) {
							token_append_child(parent, child);
						} else {
							log_error("build_from_str: accum[%zu] is NULL", idx);
						}
					} else {
						/* Not a valid sentinel — emit as text */
						/* Reconstruct the raw bytes: \0 + segment + \x7F */
						size_t raw_len= marker_len + 2;
						char *raw= malloc(raw_len + 1);
						assert(raw);
						raw[0]= '\0';
						memcpy(raw + 1, marker_content, marker_len);
						raw[raw_len - 1]= '\x7F';
						raw[raw_len]= '\0';
						token_append_text_n(parent, raw, raw_len);
						free(raw);
					}
				}

				seg_start= i + 1;
				in_marker= false;
				i++;
			} else {
				i++;
			}
		}
	}

	free(src);
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
		if(c->text && memchr(c->text, '\x7F', c->text_len)) {
			has_marker_text= true;
		}
	}

	if(has_marker_text && all_text_children) {
		char *joined= malloc(total_text_len + 1);
		assert(joined);
		size_t pos= 0;
		for(size_t j= 0; j < t->child_count; j++) {
			Child *c= &t->children[j];
			memcpy(joined + pos, c->text, c->text_len);
			pos+= c->text_len;
		}
		joined[total_text_len]= '\0';
		build_from_str(t, joined, total_text_len, accum);
		free(joined);
	}

	/* For each child of t: */
	for(size_t j= 0; j < t->child_count; j++) {
		Child *c= &t->children[j];
		if(c->is_text) {
			const char *text= c->text;
			size_t text_len= c->text_len;
			if(text && memchr(text, '\x7F', text_len)) {
				/* This token (t) has a text child with sentinels.
                 * Rebuild t from this text, which replaces all children.
                 * Note: build_from_str frees all children (including this
                 * text child) and replaces them. After this call, child
                 * positions shift, so we exit the loop. */
				build_from_str(t, text, text_len, accum);
				break;
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
	free(ac->token->name);
	ac->token->name= strdup(name);
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
