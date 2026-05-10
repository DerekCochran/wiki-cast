/*
 * Copyright (c) 2020 rxi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "util/log.h"
#include "token.h"

#include <ctype.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CALLBACKS 32

typedef struct {
	log_LogFn fn;
	void *udata;
	int level;
} Callback;

static struct {
	void *udata;
	log_LockFn lock;
	int level;
	bool quiet;
	Callback callbacks[MAX_CALLBACKS];
} L;

static pthread_once_t g_log_init_once= PTHREAD_ONCE_INIT;

static int parse_log_level(const char *s) {
	if(!s || !*s) return LOG_INFO;

	while(*s && isspace((unsigned char)*s)) s++;
	if(*s == '\0') return LOG_INFO;

	char buffer[16];
	size_t i= 0;
	while(*s && i + 1 < sizeof(buffer)) {
		if(!isspace((unsigned char)*s))
			buffer[i++]= (char)toupper((unsigned char)*s);
		s++;
	}
	buffer[i]= '\0';

	if(strcmp(buffer, "TRACE") == 0) return LOG_TRACE;
	if(strcmp(buffer, "DEBUG") == 0) return LOG_DEBUG;
	if(strcmp(buffer, "INFO") == 0) return LOG_INFO;
	if(strcmp(buffer, "WARN") == 0 || strcmp(buffer, "WARNING") == 0) return LOG_WARN;
	if(strcmp(buffer, "ERROR") == 0) return LOG_ERROR;
	if(strcmp(buffer, "FATAL") == 0) return LOG_FATAL;

	char *end;
	long parsed= strtol(buffer, &end, 10);
	if(end != buffer && *end == '\0' && parsed >= LOG_TRACE && parsed <= LOG_FATAL) {
		return (int)parsed;
	}

	return LOG_INFO;
}

static void init_log(void) {
	const char *env= getenv("TOKENIZER_LOG_LEVEL");
	L.level= parse_log_level(env);
}

static const char *level_strings[]= {
"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

#ifdef LOG_USE_COLOR
static const char *level_colors[]= {
"\x1b[94m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[35m"};
#endif

static void stdout_callback(log_Event *ev) {
	char buf[16];
	buf[strftime(buf, sizeof(buf), "%H:%M:%S", ev->time)]= '\0';
#ifdef LOG_USE_COLOR
	fprintf(
	ev->udata, "%s %s%-5s\x1b[0m \x1b[90m%s:%d:\x1b[0m ",
	buf, level_colors[ev->level], level_strings[ev->level],
	ev->file, ev->line);
#else
	fprintf(
	ev->udata, "%s %-5s %s:%d: ",
	buf, level_strings[ev->level], ev->file, ev->line);
#endif
	vfprintf(ev->udata, ev->fmt, ev->ap);
	fprintf(ev->udata, "\n");
	fflush(ev->udata);
}

static void file_callback(log_Event *ev) {
	char buf[64];
	buf[strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ev->time)]= '\0';
	fprintf(
	ev->udata, "%s %-5s %s:%d: ",
	buf, level_strings[ev->level], ev->file, ev->line);
	vfprintf(ev->udata, ev->fmt, ev->ap);
	fprintf(ev->udata, "\n");
	fflush(ev->udata);
}

static void lock(void) {
	if(L.lock) { L.lock(true, L.udata); }
}

static void unlock(void) {
	if(L.lock) { L.lock(false, L.udata); }
}

const char *log_level_string(int level) {
	return level_strings[level];
}

void log_set_lock(log_LockFn fn, void *udata) {
	L.lock= fn;
	L.udata= udata;
}

int log_get_level(void) { return L.level; }

void log_set_level(int level) {
	L.level= level;
}

void log_set_quiet(bool enable) {
	L.quiet= enable;
}

int log_add_callback(log_LogFn fn, void *udata, int level) {
	for(int i= 0; i < MAX_CALLBACKS; i++) {
		if(!L.callbacks[i].fn) {
			L.callbacks[i]= (Callback){fn, udata, level};
			return 0;
		}
	}
	return -1;
}

int log_add_fp(FILE *fp, int level) {
	return log_add_callback(file_callback, fp, level);
}

static void init_event(log_Event *ev, void *udata) {
	if(!ev->time) {
		time_t t= time(NULL);
		ev->time= localtime(&t);
	}
	ev->udata= udata;
}

void log_log(int level, const char *file, int line, const char *fmt, ...) {
	pthread_once(&g_log_init_once, init_log);

	log_Event ev= {
	.fmt= fmt,
	.file= file,
	.line= line,
	.level= level,
	};

	lock();

	if(!L.quiet && level >= L.level) {
		init_event(&ev, stderr);
		va_start(ev.ap, fmt);
		stdout_callback(&ev);
		va_end(ev.ap);
	}

	for(int i= 0; i < MAX_CALLBACKS && L.callbacks[i].fn; i++) {
		Callback *cb= &L.callbacks[i];
		if(level >= cb->level) {
			init_event(&ev, cb->udata);
			va_start(ev.ap, fmt);
			cb->fn(&ev);
			va_end(ev.ap);
		}
	}

	unlock();
}

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

void log_log_env_token(int level, const char *file, int line, const char *env, const Token* token, const char *fmt, ...) {
    pthread_once(&g_log_init_once, init_log);

    // 1. Check environment variable and serialize token early
    char *json_str = NULL;
    if (getenv(env) && token) {
		cJSON *root = token_to_json(token);
        json_str = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
    }else {
		return;
	}

    log_Event ev = {
        .fmt   = fmt,
        .file  = file,
        .line  = line,
        .level = level,
    };

    lock();

    // 2. Handle stderr output
	init_event(&ev, stderr);
	va_start(ev.ap, fmt);
	stdout_callback(&ev);
	va_end(ev.ap);
	
	if (json_str) {
		fprintf(stderr, "   --> %s", json_str);
	}
	fprintf(stderr, "\n"); // Ensure newline after the extra token data

    // 3. Handle all other callbacks (e.g., file logging)
    for (int i = 0; i < MAX_CALLBACKS && L.callbacks[i].fn; i++) {
        Callback *cb = &L.callbacks[i];
        if (level >= cb->level) {
            init_event(&ev, cb->udata);
            va_start(ev.ap, fmt);
            cb->fn(&ev);
            va_end(ev.ap);
            
            if (json_str) {
                fprintf((FILE*)cb->udata, " | Token: %s", json_str);
            }
            fprintf((FILE*)cb->udata, "\n");
        }
    }

    unlock();

    // 4. Clean up allocated JSON string
    if (json_str) {
        free(json_str);
    }
}

