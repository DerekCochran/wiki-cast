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
#include "util/env_cache.h"

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
	 const char *env = env_get_n("TOKENIZER_LOG_LEVEL", sizeof("TOKENIZER_LOG_LEVEL") - 1);
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

void log_log_env_token(int level, const char *file, int line, const char *env, const Token* token, const char *fmt, ...) {
    pthread_once(&g_log_init_once, init_log);

    // 1. Check environment variable and serialize token early
	if( !env_set(env) ) {
		return;
	}
    char *json_str = NULL;
    if (token) {
		cJSON *root = token_to_json(token);
        json_str = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
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

