/*
 * thread_buffer.c — Per-thread reusable buffer management.
 *
 * See thread_buffer.h for the full design description.
 */
#include "util/thread_buffer.h"
#include "util/log.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

static size_t align_up_64(size_t n) {
	return (n + 63u) & ~(size_t)63u;
}

static void *aligned_zalloc_64(size_t n) {
	size_t alloc_n= align_up_64(n);
	void *p= aligned_alloc(64, alloc_n);
	assert(p);
	memset(p, 0, alloc_n);
	return p;
}

/* ── Default thresholds (in megabytes) ───────────────────────────────────── */
#define DEFAULT_MAIN_SHRINK_MB 5
#define DEFAULT_MAIN_TARGET_MB 1
#define DEFAULT_SCRATCH_SHRINK_MB 5
#define DEFAULT_SCRATCH_TARGET_MB 1

/* Byte equivalents, filled in by init_thresholds(). */
static size_t g_main_shrink_bytes;
static size_t g_main_target_bytes;
static size_t g_scratch_shrink_bytes;
static size_t g_scratch_target_bytes;

#define INITIAL_SCRATCH_POOL_CAP 4

#define SCRATCH_BUCKET_1K_CAP ((size_t)1024u)
#define SCRATCH_BUCKET_1M_CAP ((size_t)(1024u * 1024u))
#define INVALID_INDEX WIKI_THREAD_BUF_INVALID_INDEX
#define SCRATCH_BUCKET_COUNT WIKI_THREAD_SCRATCH_BUCKET_COUNT

typedef enum {
	SCRATCH_BUCKET_SMALL = 0,
	SCRATCH_BUCKET_64_TO_1K = 1,
	SCRATCH_BUCKET_1K_TO_1M = 2,
	SCRATCH_BUCKET_GT_1M = 3
} ScratchBucket;

/* ── Env-var helpers ─────────────────────────────────────────────────────── */

/* Read an env var as a positive integer (megabytes); fall back to default. */
static size_t read_env_mb(const char *name, size_t default_mb) {
	const char *v= getenv(name);
	if(!v || !*v) return default_mb;
	char *end;
	long val= strtol(v, &end, 10);
	if(*end != '\0' || val <= 0) {
		log_warn("thread_buffer: invalid value for %s='%s'; using default %zu MB",
						 name, v, default_mb);
		return default_mb;
	}
	return (size_t)val;
}

/* Initialise threshold globals exactly once. */
static pthread_once_t g_thresholds_once= PTHREAD_ONCE_INIT;

static void init_thresholds(void) {
	size_t ms= read_env_mb("TOKENIZER_THREAD_BUFFER_MAIN_SHRINK_SIZE_MB",
												 DEFAULT_MAIN_SHRINK_MB);
	size_t mt= read_env_mb("TOKENIZER_THREAD_BUFFER_MAIN_TARGET_SIZE_MB",
												 DEFAULT_MAIN_TARGET_MB);
	size_t ss= read_env_mb("TOKENIZER_THREAD_BUFFER_SCRATCH_SHRINK_SIZE_MB",
												 DEFAULT_SCRATCH_SHRINK_MB);
	size_t st= read_env_mb("TOKENIZER_THREAD_BUFFER_SCRATCH_TARGET_SIZE_MB",
												 DEFAULT_SCRATCH_TARGET_MB);

	g_main_shrink_bytes= ms * 1024 * 1024;
	g_main_target_bytes= mt * 1024 * 1024;
	g_scratch_shrink_bytes= ss * 1024 * 1024;
	g_scratch_target_bytes= st * 1024 * 1024;
}

/* ── Global registry of all ThreadBuffers (for finalize_all) ─────────────── */
/*
 * Every ThreadBuffers that is allocated is added to a singly-linked list
 * protected by g_registry_mutex.  This allows finalize_all() to reach
 * every live ThreadBuffers without knowing which threads exist.
 */

typedef struct RegNode {
	ThreadBuffers *tb;
	struct RegNode *next;
} RegNode;

static pthread_mutex_t g_registry_mutex= PTHREAD_MUTEX_INITIALIZER;
static RegNode *g_registry_head= NULL;

static void registry_add(ThreadBuffers *tb) {
	RegNode *node= malloc(sizeof(RegNode));
	assert(node);
	node->tb= tb;
	pthread_mutex_lock(&g_registry_mutex);
	node->next= g_registry_head;
	g_registry_head= node;
	pthread_mutex_unlock(&g_registry_mutex);
}

const char *wiki_thread_buf_append_to_tokens(const char *s, size_t len) {
	if(!s || len == 0) return NULL;
	/* Temporary mode: token_append_text_n() now duplicates text into owned memory,
	 * so we can avoid using the shared tokens arena and pointer lifetime hazards. */
	return s;
}

const char *wiki_thread_buf_append_view_to_tokens(sz_string_view_t view) {
	if(view.length == 0 || !view.start) return NULL;
	return view.start;
}

/* Remove the registry entry for tb (called from the TLS destructor). */
static void registry_remove(ThreadBuffers *tb) {
	pthread_mutex_lock(&g_registry_mutex);
	RegNode **pp= &g_registry_head;
	while(*pp) {
		if((*pp)->tb == tb) {
			RegNode *dead= *pp;
			*pp= dead->next;
			free(dead);
			break;
		}
		pp= &(*pp)->next;
	}
	pthread_mutex_unlock(&g_registry_mutex);
}

/* ── Compiler TLS cache + pthread TLS key ───────────────────────────────── */

static THREAD_LOCAL ThreadBuffers *g_tls_buffers= NULL;
static pthread_key_t g_tls_key;
static pthread_once_t g_key_once= PTHREAD_ONCE_INIT;

static void free_thread_buf(ThreadBuf *tb);
static void free_scratch_pool(ThreadBuffers *tb);

/*
 * TLS destructor: called automatically when a thread exits.
 * Frees inner buffers unless finalize_all() already did so, then frees
 * the ThreadBuffers struct itself.
 */
static void thread_buffers_destructor(void *ptr) {
	if(!ptr) return;
	ThreadBuffers *tb= (ThreadBuffers *)ptr;

	/* Remove from registry (no-op if finalize_all already cleared it). */
	registry_remove(tb);

	/* Only free inner buffers if finalize_all() has not already done so. */
	if(!tb->finalized) {
		free_thread_buf(&tb->stage);
		free_thread_buf(&tb->tokens);
		free_scratch_pool(tb);
	}

	/* Always free the struct itself — finalize_all() intentionally leaves it
     * alive so this destructor can clean up safely. */
	free(tb);
}

static void create_tls_key(void) {
	pthread_key_create(&g_tls_key, thread_buffers_destructor);
}

static void init_thread_buf(ThreadBuf *tb, size_t shrink_size, size_t target_size) {
    tb->shrink_size = shrink_size;
    tb->target_size = target_size;
	tb->pool_index = INVALID_INDEX;
    tb->len = 0;

    /* Start with SSO: point buf at inline storage, no heap allocation needed. */
    tb->buf = tb->inline_data;
    tb->cap = sizeof(tb->inline_data);
    tb->is_on_heap = false;
	tb->sso_allowed = true;
	tb->shrink_allowed = true;
    tb->inline_data[0] = '\0';
}

static void free_thread_buf(ThreadBuf *tb) {
	if(!tb) return;
	if(tb->is_on_heap && tb->buf) {
		free(tb->buf);
	}
	/* Reset to SSO state so the struct is safe to reuse or ignore. */
	tb->buf = tb->inline_data;
	tb->cap = sizeof(tb->inline_data);
	tb->is_on_heap = false;
	tb->sso_allowed = true;
	tb->shrink_allowed = true;
	tb->pool_index = INVALID_INDEX;
	tb->len = 0;
}

static void scratch_reset_free_heads(ThreadBuffers *tb) {
	for(size_t i= 0; i < SCRATCH_BUCKET_COUNT; i++) {
		tb->scratch_free_head[i]= INVALID_INDEX;
	}
}

static size_t scratch_actual_need(size_t len) {
	if(len == INVALID_INDEX) return INVALID_INDEX;
	return len + 1;
}

static ScratchBucket scratch_bucket_for_need(size_t len) {
	size_t actual_need= scratch_actual_need(len);
	if(actual_need <= WIKI_THREAD_BUF_INLINE_CAP) return SCRATCH_BUCKET_SMALL;
	if(actual_need <= SCRATCH_BUCKET_1K_CAP) return SCRATCH_BUCKET_64_TO_1K;
	if(actual_need <= SCRATCH_BUCKET_1M_CAP) return SCRATCH_BUCKET_1K_TO_1M;
	return SCRATCH_BUCKET_GT_1M;
}

static ScratchBucket scratch_heap_bucket_for_cap(size_t cap) {
	if(cap <= SCRATCH_BUCKET_1K_CAP) return SCRATCH_BUCKET_64_TO_1K;
	if(cap <= SCRATCH_BUCKET_1M_CAP) return SCRATCH_BUCKET_1K_TO_1M;
	return SCRATCH_BUCKET_GT_1M;
}

static ScratchBucket scratch_bucket_for_buffer(const ThreadBuf *scratch) {
	if(!scratch || !scratch->is_on_heap) return SCRATCH_BUCKET_SMALL;
	return scratch_heap_bucket_for_cap(scratch->cap);
}

static void scratch_free_push(ThreadBuffers *tb, size_t idx, ScratchBucket bucket) {
	assert(bucket < SCRATCH_BUCKET_COUNT);
	tb->scratch_bucket[idx]= (unsigned char)bucket;
	tb->scratch_next_free[idx]= tb->scratch_free_head[bucket];
	tb->scratch_free_head[bucket]= idx;
}

static size_t scratch_free_pop_bucket(ThreadBuffers *tb, ScratchBucket bucket, size_t avoid_idx) {
	assert(bucket < SCRATCH_BUCKET_COUNT);
	size_t head= tb->scratch_free_head[bucket];
	if(head == INVALID_INDEX) return INVALID_INDEX;
	assert(head < tb->scratch_count);
	assert(tb->scratch_bucket[head] == (unsigned char)bucket);

	if(head != avoid_idx) {
		tb->scratch_free_head[bucket]= tb->scratch_next_free[head];
		tb->scratch_next_free[head]= INVALID_INDEX;
		return head;
	}

	size_t second= tb->scratch_next_free[head];
	if(second == INVALID_INDEX) {
		return INVALID_INDEX;
	}
	assert(second < tb->scratch_count);
	assert(tb->scratch_bucket[second] == (unsigned char)bucket);
	tb->scratch_next_free[head]= tb->scratch_next_free[second];
	tb->scratch_next_free[second]= INVALID_INDEX;
	return second;
}

static size_t scratch_acquire_index(ThreadBuffers *tb, ScratchBucket desired_bucket, size_t avoid_idx) {
	size_t idx= scratch_free_pop_bucket(tb, desired_bucket, avoid_idx);
	if(idx != INVALID_INDEX) {
		return idx;
	}

	if(desired_bucket == SCRATCH_BUCKET_SMALL) {
		idx= scratch_free_pop_bucket(tb, SCRATCH_BUCKET_64_TO_1K, avoid_idx);
		if(idx != INVALID_INDEX) {
			return idx;
		}
		idx= scratch_free_pop_bucket(tb, SCRATCH_BUCKET_1K_TO_1M, avoid_idx);
		if(idx != INVALID_INDEX) {
			return idx;
		}
		return scratch_free_pop_bucket(tb, SCRATCH_BUCKET_GT_1M, avoid_idx);
	}

	if(desired_bucket == SCRATCH_BUCKET_64_TO_1K) {
		idx= scratch_free_pop_bucket(tb, SCRATCH_BUCKET_1K_TO_1M, avoid_idx);
		if(idx != INVALID_INDEX) {
			return idx;
		}
		return scratch_free_pop_bucket(tb, SCRATCH_BUCKET_GT_1M, avoid_idx);
	}

	if(desired_bucket == SCRATCH_BUCKET_1K_TO_1M) {
		return scratch_free_pop_bucket(tb, SCRATCH_BUCKET_GT_1M, avoid_idx);
	}

	return INVALID_INDEX;
}

static void ensure_scratch_capacity(ThreadBuffers *tb, size_t min_cap) {
	if(tb->scratch_cap >= min_cap) return;

	size_t new_cap= tb->scratch_cap ? tb->scratch_cap * 2 : INITIAL_SCRATCH_POOL_CAP;
	while(new_cap < min_cap) {
		new_cap*= 2;
	}

	ThreadBuf **new_pool= realloc(tb->scratch_pool, new_cap * sizeof(ThreadBuf *));
	bool *new_in_use= realloc(tb->scratch_in_use, new_cap * sizeof(bool));
	size_t *new_next_free= realloc(tb->scratch_next_free, new_cap * sizeof(size_t));
	unsigned char *new_bucket= realloc(tb->scratch_bucket, new_cap * sizeof(unsigned char));
	assert(new_pool && new_in_use && new_next_free && new_bucket);

	tb->scratch_pool= new_pool;
	tb->scratch_in_use= new_in_use;
	tb->scratch_next_free= new_next_free;
	tb->scratch_bucket= new_bucket;

	for(size_t i= tb->scratch_cap; i < new_cap; i++) {
		tb->scratch_pool[i]= NULL;
		tb->scratch_in_use[i]= false;
		tb->scratch_next_free[i]= INVALID_INDEX;
		tb->scratch_bucket[i]= (unsigned char)SCRATCH_BUCKET_SMALL;
	}

	tb->scratch_cap= new_cap;
}

static size_t scratch_alloc_entry(ThreadBuffers *tb) {
	ensure_scratch_capacity(tb, tb->scratch_count + 1);

	size_t idx= tb->scratch_count++;
	tb->scratch_pool[idx]= aligned_zalloc_64(sizeof(ThreadBuf));
	assert(tb->scratch_pool[idx]);
	init_thread_buf(tb->scratch_pool[idx], g_scratch_shrink_bytes, g_scratch_target_bytes);
	tb->scratch_pool[idx]->pool_index= idx;
	tb->scratch_in_use[idx]= false;
	tb->scratch_next_free[idx]= INVALID_INDEX;
	tb->scratch_bucket[idx]= (unsigned char)SCRATCH_BUCKET_SMALL;
	return idx;
}

static size_t scratch_find_owner_idx(ThreadBuffers *tb, const char *ptr) {
	if(!tb || !ptr) return INVALID_INDEX;
	uintptr_t p= (uintptr_t)ptr;
	for(size_t i= 0; i < tb->scratch_count; i++) {
		if(!tb->scratch_in_use[i]) continue;
		ThreadBuf *scratch= tb->scratch_pool[i];
		if(!scratch || !scratch->buf || scratch->cap == 0) continue;
		uintptr_t b= (uintptr_t)scratch->buf;
		uintptr_t e= b + scratch->cap;
		if(p >= b && p < e) return i;
	}
	return INVALID_INDEX;
}

static void free_scratch_pool(ThreadBuffers *tb) {
	if(!tb) return;
	for(size_t i= 0; i < tb->scratch_count; i++) {
		if(tb->scratch_pool[i]) {
			free_thread_buf(tb->scratch_pool[i]);
			free(tb->scratch_pool[i]);
		}
	}
	free(tb->scratch_pool);
	free(tb->scratch_in_use);
	free(tb->scratch_next_free);
	free(tb->scratch_bucket);
	tb->scratch_pool= NULL;
	tb->scratch_in_use= NULL;
	tb->scratch_next_free= NULL;
	tb->scratch_bucket= NULL;
	tb->scratch_count= 0;
	tb->scratch_cap= 0;
	scratch_reset_free_heads(tb);
}

/* ── Inner buffer allocation ─────────────────────────────────────────────── */

/*
 * Allocate inner buffers at their initial (target) sizes and stamp each
 * ThreadBuf with its policy fields.  Called on first use and after
 * finalize_all() when a thread re-enters the parse path.
 */
static void alloc_inner_buffers(ThreadBuffers *tb) {
	init_thread_buf(&tb->stage, g_main_shrink_bytes, g_main_target_bytes);
	init_thread_buf(&tb->tokens, g_main_shrink_bytes, g_main_target_bytes);
	tb->tokens.shrink_allowed= false;
	tb->scratch_pool= NULL;
	tb->scratch_in_use= NULL;
	tb->scratch_next_free= NULL;
	tb->scratch_bucket= NULL;
	tb->scratch_count= 0;
	tb->scratch_cap= 0;
	scratch_reset_free_heads(tb);

	tb->finalized= false;
}

void wiki_thread_buf_assert_no_leased_scratch(const char *context, const ThreadBuf *ignore_tb) {
	ThreadBuffers *tb= wiki_thread_buf_get();
	size_t leased_count= 0;
	size_t leased_cap= 0;
	size_t leased_len= 0;
	char details[512];
	size_t used= 0;

	details[0]= '\0';

	for(size_t i= 0; i < tb->scratch_count; i++) {
		ThreadBuf *scratch= tb->scratch_pool[i];
		if(!tb->scratch_in_use[i] || scratch == ignore_tb) {
			continue;
		}

		leased_count++;
		leased_cap+= scratch->cap;
		leased_len+= scratch->len;

		if(used < sizeof(details)) {
			/* Print first 32 bytes of content for diagnostics */
			char content_hex[128];
			size_t ch_used = 0;
			size_t show = scratch->len < 32 ? scratch->len : 32;
			for(size_t ci = 0; ci < show && ch_used + 4 < sizeof(content_hex); ci++) {
				unsigned char cc = (unsigned char)scratch->buf[ci];
				if(cc >= 32 && cc < 127 && cc != '\\') {
					content_hex[ch_used++] = (char)cc;
				} else {
					int wc = snprintf(content_hex + ch_used, sizeof(content_hex) - ch_used, "\\x%02X", cc);
					if(wc > 0) ch_used += (size_t)wc;
				}
			}
			content_hex[ch_used] = '\0';
			int wrote= snprintf(details + used, sizeof(details) - used,
													"%s#%zu(cap=%zu,len=%zu,content=[%s])",
													used == 0 ? "" : ", ",
													i, scratch->cap, scratch->len, content_hex);
			if(wrote > 0) {
				size_t wrote_sz= (size_t)wrote;
				used+= wrote_sz < (sizeof(details) - used)
							 ? wrote_sz
							 : (sizeof(details) - used);
			}
		}
	}

	if(leased_count == 0) {
		return;
	}

	log_fatal("%s: %zu leased scratch buffers remain (total_cap=%zu total_len=%zu)%s%s",
						context ? context : "thread_buffer",
						leased_count,
						leased_cap,
						leased_len,
						details[0] ? "; " : "",
						details);
	abort();
}

/* Helper: build an escaped, human-readable representation of a binary buffer.
 * Escapes NUL as "\\0" and DEL (0x7F) as "\\x7F". Other non-printable
 * bytes are emitted as "\\xHH". Output is truncated to out_cap-1 bytes.
 */
static void build_escaped_repr(const char *buf, size_t len, char *out, size_t out_cap) {
	if(!out || out_cap == 0) return;
	size_t op = 0;
	for(size_t i = 0; i < len && op + 8 < out_cap; i++) {
		unsigned char c = (unsigned char)buf[i];
		if(c == '\0') {
			int n = snprintf(out + op, out_cap - op, "\\0");
			if(n > 0) op += (size_t)n;
		} else if(c == 0x7F) {
			int n = snprintf(out + op, out_cap - op, "\\x7F");
			if(n > 0) op += (size_t)n;
		} else if(c >= 32 && c < 127 && c != '\\') {
			out[op++] = (char)c;
		} else {
			int n = snprintf(out + op, out_cap - op, "\\x%02X", c);
			if(n > 0) op += (size_t)n;
		}
	}
	if(op >= out_cap) op = out_cap - 1;
	if(op < out_cap) {
		if(len > 0 && op + 16 < out_cap) {
			/* If we truncated, indicate so */
			if(op + 14 < out_cap) {
				int n = snprintf(out + op, out_cap - op, "...(trunc)");
				if(n > 0) op += (size_t)n;
			}
		}
		out[op] = '\0';
	}
}

/* Helper: build a sentinel-aware representation of tokens arena.
 * Replaces sequences of the form \0<digits><type>\x7F with "[#<digits>:<type>]".
 * Other bytes are escaped like build_escaped_repr. Output truncated to out_cap.
 */
static void build_sentinel_repr(const char *buf, size_t len, char *out, size_t out_cap) {
	if(!out || out_cap == 0) return;
	size_t op = 0;
	size_t i = 0;
	while(i < len && op + 8 < out_cap) {
		unsigned char c = (unsigned char)buf[i];
		if(c == '\0') {
			size_t p = i + 1;
			size_t digits_start = p;
			while(p < len && buf[p] >= '0' && buf[p] <= '9') p++;
			if(p > digits_start && p < len) {
				unsigned char type_ch = (unsigned char)buf[p];
				if(p + 1 < len && (unsigned char)buf[p + 1] == 0x7F) {
					/* matched sentinel */
					size_t dlen = p - digits_start;
					if(dlen < 64) {
						char idxbuf[80];
						memcpy(idxbuf, buf + digits_start, dlen);
						idxbuf[dlen] = '\0';
						if(type_ch >= 32 && type_ch < 127) {
							int n = snprintf(out + op, out_cap - op, "[#%s:%c]", idxbuf, (char)type_ch);
							if(n > 0) op += (size_t)n;
						} else {
							int n = snprintf(out + op, out_cap - op, "[#%s:0x%02X]", idxbuf, type_ch);
							if(n > 0) op += (size_t)n;
						}
						i = p + 2;
						continue;
					}
				}
			}
			/* fallback: emit escaped NUL */
			int n = snprintf(out + op, out_cap - op, "\\0");
			if(n > 0) op += (size_t)n;
			i++;
		} else if(c >= 32 && c < 127 && c != '\\') {
			out[op++] = (char)c;
			i++;
		} else if(c == 0x7F) {
			int n = snprintf(out + op, out_cap - op, "\\x7F");
			if(n > 0) op += (size_t)n;
			i++;
		} else {
			int n = snprintf(out + op, out_cap - op, "\\x%02X", c);
			if(n > 0) op += (size_t)n;
			i++;
		}
	}
	if(op >= out_cap) op = out_cap - 1;
	if(op < out_cap) {
		if(i < len && op + 12 < out_cap) {
			int n = snprintf(out + op, out_cap - op, "...(trunc)");
			if(n > 0) op += (size_t)n;
		}
		out[op] = '\0';
	}
}

void wiki_thread_buf_log_state(const char *stage_label, ThreadBuf *stage_tb) {
	if(!getenv("WTC_DEBUG_STAGE_DUMP")) return;
	if(!stage_label) stage_label = "(stage)";

	ThreadBuffers *tbs = wiki_thread_buf_get();
	if(!tbs) return;
	ThreadBuf *tokens = &tbs->tokens;

	/* Allocate bounded buffers for escaping. Make the max configurable via
	 * WTC_DEBUG_STAGE_DUMP_MAX (bytes). Default to 64KiB to allow long
	 * dumps while keeping output reasonable. */
	const char *maxenv = getenv("WTC_DEBUG_STAGE_DUMP_MAX");
	size_t MAX_OUT = 65536; /* default 64KiB */
	if(maxenv && *maxenv) {
		char *endptr = NULL;
		long v = strtol(maxenv, &endptr, 10);
		if(endptr && *endptr == '\0' && v > 0) {
			MAX_OUT = (size_t)v;
		}
	}
	size_t s_cap = stage_tb && stage_tb->len ? (stage_tb->len * 4 + 32) : 128;
	if(s_cap > MAX_OUT) s_cap = MAX_OUT;
	char *sbuf = malloc(s_cap + 1);
	if(!sbuf) return;
	size_t stage_len = stage_tb ? stage_tb->len : 0;
	const char *stage_buf = stage_tb ? stage_tb->buf : "";
	build_escaped_repr(stage_buf, stage_len, sbuf, s_cap);
	log_debug_env_token("WTC_DEBUG_STAGE_DUMP", NULL,
		"[C DUMP_STAGE] %s ptr=%p len=%zu cap=%zu data=%s",
		stage_label, (void*)stage_buf, stage_len, stage_tb ? stage_tb->cap : 0, sbuf);

	size_t t_cap = tokens->len ? (tokens->len * 6 + 64) : 128;
	if(t_cap > MAX_OUT) t_cap = MAX_OUT;
	char *tbuf = malloc(t_cap + 1);
	if(!tbuf) { free(sbuf); return; }
	build_sentinel_repr(tokens->buf, tokens->len, tbuf, t_cap);
	log_debug_env_token("WTC_DEBUG_STAGE_DUMP", NULL,
		"[C DUMP_TOKENS] ptr=%p len=%zu cap=%zu repr=%s",
		(void*)tokens->buf, tokens->len, tokens->cap, tbuf);

	free(sbuf);
	free(tbuf);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

/*
 * The single authority for all ThreadBuf resize decisions.
 *
 * Shrink: cap is wastefully large AND the new request is small → free and
 *         reallocate at target_size.  After the shrink, cap == target_size
 *         which is ≥ need+1 (because need < target_size), so no further
 *         action is needed.
 *
 * Grow:   buffer cannot hold `need` bytes → double until large enough.
 *
 * No-op:  current capacity already satisfies `need`.
 *
 * Callers must never realloc a ThreadBuf buffer directly; always use this.
 */
void wiki_thread_buf_reserve_ex(ThreadBuf *tb, size_t need, bool sso_allowed) {
	if(!tb) return;

	size_t actual_need = need + 1; /* +1 for null terminator */
	bool allow_sso = sso_allowed && tb->sso_allowed;

	/* If caller disallows SSO and this buffer is currently inline, migrate to heap. */
	if(!allow_sso && !tb->is_on_heap) {
		size_t new_cap= tb->target_size;
		if(new_cap < actual_need) new_cap = actual_need;
		new_cap = (new_cap + 63) & ~((size_t)63);

		void *new_ptr= aligned_alloc(64, new_cap);
		assert(new_ptr);
		if(tb->len > 0) {
			memcpy(new_ptr, tb->buf, tb->len);
		}
		tb->buf = (char *)new_ptr;
		tb->cap = new_cap;
		tb->is_on_heap = true;
		log_trace("thread_buffer force-heap: need=%zu, new_cap=%zu", need, new_cap);
	}

	if (tb->shrink_allowed && tb->is_on_heap && tb->cap > tb->shrink_size && need < tb->target_size) {
		free(tb->buf);
		if (allow_sso && actual_need <= sizeof(tb->inline_data)) {
			tb->buf = tb->inline_data;
			tb->cap = sizeof(tb->inline_data);
			tb->is_on_heap = false;
		} else {
			size_t alloc_sz = (tb->target_size + 63) & ~63;
			if(alloc_sz < actual_need) {
				alloc_sz = (actual_need + 63) & ~((size_t)63);
			}
			tb->buf = aligned_alloc(64, alloc_sz);
			assert(tb->buf);
			tb->cap = alloc_sz;
			tb->is_on_heap = true;
		}
		log_trace("thread_buffer shrink: new_cap=%zu", tb->cap);
		return;
	}

    /* 2. Grow: current buffer cannot hold `need` bytes. */
    if (tb->cap < actual_need) {
        /* Can the new content fit in the inline SSO buffer? */
		if (allow_sso && actual_need <= sizeof(tb->inline_data)) {
            if (tb->is_on_heap) {
                free(tb->buf);
                tb->is_on_heap = false;
            }
            tb->buf = tb->inline_data;
            tb->cap = sizeof(tb->inline_data);
            return;
        }

        size_t new_cap;
        if (need > tb->shrink_size) {
            /* Large request: allocate exactly what is needed. */
            new_cap = actual_need;
        } else {
            /* Medium request: double to amortise reallocations. */
            new_cap = tb->cap > 0 ? tb->cap * 2 : tb->target_size;
            if (new_cap < actual_need) new_cap = actual_need;
        }

        /* 64-byte alignment for SIMD. */
        new_cap = (new_cap + 63) & ~63;

        void *new_ptr = aligned_alloc(64, new_cap);
        assert(new_ptr);

        if (tb->len > 0) {
            memcpy(new_ptr, tb->buf, tb->len);
        }

        if (tb->is_on_heap) {
			free(tb->buf);
        }

        tb->buf = (char *)new_ptr;
        tb->cap = new_cap;
        tb->is_on_heap = true;

        log_trace("thread_buffer grow: need=%zu, new_cap=%zu", need, new_cap);
    }
}

void wiki_thread_buf_reserve(ThreadBuf *tb, size_t need) {
	wiki_thread_buf_reserve_ex(tb, need, true);
}

ThreadBuffers *wiki_thread_buf_get(void) {
	/* Fast path: TLS cache already populated and not finalized — skip pthread_once. */
	ThreadBuffers *tb= g_tls_buffers;
	if(__builtin_expect(tb != NULL && !tb->finalized, 1)) {
		return tb;
	}

	pthread_once(&g_key_once, create_tls_key);
	pthread_once(&g_thresholds_once, init_thresholds);

	if(!tb) {
		tb= (ThreadBuffers *)pthread_getspecific(g_tls_key);
	}

	if(!tb) {
		/* First call on this thread: allocate the struct and register it. */
		tb= aligned_zalloc_64(sizeof(ThreadBuffers));
		assert(tb);
		alloc_inner_buffers(tb);
		pthread_setspecific(g_tls_key, tb);
		g_tls_buffers= tb;
		registry_add(tb);
	} else if(tb->finalized) {
		/*
         * finalize_all() ran while this thread was alive: inner buffers were
         * freed and the struct was left in TLS.  Re-initialise in place and
         * re-add to the registry so future finalize_all() calls find it.
         */
		alloc_inner_buffers(tb);
		pthread_setspecific(g_tls_key, tb);
		g_tls_buffers= tb;
		registry_add(tb);
	} else {
		/* Cache pointer in compiler TLS for fast future lookups. */
		g_tls_buffers= tb;
	}

	return tb;
}

static ThreadBuf *acquire_scratch_with_len_internal(ThreadBuffers *tb, size_t len, size_t avoid_idx) {
	ScratchBucket desired_bucket= scratch_bucket_for_need(len);
	size_t idx= scratch_acquire_index(tb, desired_bucket, avoid_idx);

	if(idx == INVALID_INDEX) {
		idx= scratch_alloc_entry(tb);
	}

	assert(idx < tb->scratch_count);
	ThreadBuf *scratch= tb->scratch_pool[idx];
	assert(scratch);
	assert(scratch->pool_index == idx);
	tb->scratch_in_use[idx]= true;
	tb->scratch_next_free[idx]= INVALID_INDEX;

	/* Reserve only what this call needs to avoid inflating pool buffers. */
	wiki_thread_buf_reserve(scratch, len);
	scratch->len= 0;
	if(scratch->cap > 0) {
		scratch->buf[0]= '\0';
	}

	return scratch;
}

ThreadBuf *wiki_thread_buf_acquire_scratch_with_len(size_t len) {
	ThreadBuffers *tb= wiki_thread_buf_get();
	return acquire_scratch_with_len_internal(tb, len, INVALID_INDEX);
}

ThreadBuf *wiki_thread_buf_acquire_scratch_from_data(const char *s, size_t len) {
	ThreadBuffers *tb= wiki_thread_buf_get();
	size_t avoid_idx= scratch_find_owner_idx(tb, s);
	ThreadBuf *scratch= acquire_scratch_with_len_internal(tb, len, avoid_idx);

	if(len > 0 && s != NULL) {
		memmove(scratch->buf, s, len);
	}

	scratch->buf[len]= '\0';
	scratch->len= len;
	return scratch;
}

ThreadBuf *wiki_thread_buf_acquire_scratch(void) {
	return wiki_thread_buf_acquire_scratch_with_len(0);
}

void wiki_thread_buf_release_scratch(ThreadBuf *scratch) {
	if(!scratch) return;

	ThreadBuffers *tb= wiki_thread_buf_get();
	size_t idx= scratch->pool_index;
	if(idx == INVALID_INDEX || idx >= tb->scratch_count || tb->scratch_pool[idx] != scratch) {
		assert(!"wiki_thread_buf_release_scratch called with non-pooled buffer");
		return;
	}

	if(!tb->scratch_in_use[idx]) {
		log_fatal("wiki_thread_buf_release_scratch: double release (idx=%zu)", idx);
		abort();
	}

	tb->scratch_in_use[idx]= false;
	scratch->len= 0;
	if(scratch->cap > 0) {
		scratch->buf[0]= '\0';
	}

	ScratchBucket bucket= scratch_bucket_for_buffer(scratch);
	scratch_free_push(tb, idx, bucket);
}

void wiki_thread_buf_set(ThreadBuf *tb, const char *s, size_t len) {
	wiki_thread_buf_reserve(tb, len);
	if(len > 0) memmove(tb->buf, s, len);
	tb->buf[len]= '\0';
	tb->len= len;
}

void wiki_thread_buf_append(ThreadBuf *tb, sz_string_view_t view) {
	if(!tb) return;
	if(view.length == 0) return;

	/* Ensure capacity for existing content + view.length bytes */
	wiki_thread_buf_reserve(tb, tb->len + view.length);

	if(view.start) memcpy(tb->buf + tb->len, view.start, view.length);
	tb->len += view.length;
	tb->buf[tb->len] = '\0';
}

void wiki_thread_buf_putc(ThreadBuf *tb, char ch) {
	if(!tb) return;
	/* Reserve space for one additional byte */
	wiki_thread_buf_reserve(tb, tb->len + 1);
	tb->buf[tb->len++] = ch;
	tb->buf[tb->len] = '\0';
}

void wiki_thread_buf_finalize_all(void) {
	/*
     * Ensure thresholds are initialised so that any subsequent
     * wiki_thread_buf_get() call after finalize_all() uses correct sizes.
     */
	pthread_once(&g_thresholds_once, init_thresholds);

	pthread_mutex_lock(&g_registry_mutex);

	RegNode *node= g_registry_head;
	while(node) {
		RegNode *next= node->next;
		ThreadBuffers *tb= node->tb;

		/*
         * Mark finalized BEFORE freeing the buffers so that if the TLS
         * destructor fires concurrently it sees the flag and skips the free.
         * (finalize_all() is a shutdown function; concurrent parse calls
         * after this point are a caller error.)
         */
		tb->finalized= true;

		free_thread_buf(&tb->stage);
		free_thread_buf(&tb->tokens);
		free_scratch_pool(tb);

		/*
         * Do NOT free(tb): the ThreadBuffers struct is still referenced by
         * each thread's TLS slot.  The TLS destructor will free it when the
         * thread eventually exits.
         */
		free(node);
		node= next;
	}

	g_registry_head= NULL;
	pthread_mutex_unlock(&g_registry_mutex);
}

