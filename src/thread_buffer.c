/*
 * thread_buffer.c — Per-thread reusable buffer management.
 *
 * See thread_buffer.h for the full design description.
 */
#include "thread_buffer.h"
#include "log.h"

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Use 48 for inline to keep the total ThreadBuf struct exactly 1 cache line (64 bytes) */
#define SSO_MAX 47

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
	ThreadBuffers *tbs = wiki_thread_buf_get();
	ThreadBuf *tb = &tbs->tokens;
	size_t off = tb->len;
	if(tb->len + len + 1 > tb->cap) {
		log_debug("tokens arena GROW: cap=%zu len=%zu append=%zu — pointer invalidation imminent; append='%.*s'",
		          tb->cap, tb->len, len, (int)(len > 80 ? 80 : len), s);
	}
	sz_string_view_t v = { .start = s, .length = len };
	wiki_thread_buf_append(tb, v);
	return tb->buf + off;
}

const char *wiki_thread_buf_append_view_to_tokens(sz_string_view_t view) {
	if(view.length == 0 || !view.start) return NULL;
	ThreadBuffers *tbs = wiki_thread_buf_get();
	ThreadBuf *tb = &tbs->tokens;
	size_t off = tb->len;
	wiki_thread_buf_append(tb, view);
	return tb->buf + off;
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
    tb->len = 0;

    /* Start with SSO: point buf at inline storage, no heap allocation needed. */
    tb->buf = tb->inline_data;
    tb->cap = sizeof(tb->inline_data);
    tb->is_on_heap = false;
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
	tb->len = 0;
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
	tb->scratch_pool= NULL;
	tb->scratch_in_use= NULL;
	tb->scratch_count= 0;
	tb->scratch_cap= 0;
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
	tb->scratch_pool= NULL;
	tb->scratch_in_use= NULL;
	tb->scratch_count= 0;
	tb->scratch_cap= 0;

	tb->finalized= false;
}

void wiki_thread_buf_assert_no_leased_scratch(const char *context,
																							const ThreadBuf *ignore_tb) {
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
void wiki_thread_buf_reserve(ThreadBuf *tb, size_t need) {
    size_t actual_need = need + 1; /* +1 for null terminator */

    /* 1. Shrink: on heap, cap is wastefully large, and new need is small. */
    if (tb->is_on_heap && tb->cap > tb->shrink_size && need < tb->target_size) {
        free(tb->buf);
        if (actual_need <= sizeof(tb->inline_data)) {
            tb->buf = tb->inline_data;
            tb->cap = sizeof(tb->inline_data);
            tb->is_on_heap = false;
        } else {
            size_t alloc_sz = (tb->target_size + 63) & ~63;
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
        if (actual_need <= sizeof(tb->inline_data)) {
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

ThreadBuffers *wiki_thread_buf_get(void) {
	pthread_once(&g_key_once, create_tls_key);
	pthread_once(&g_thresholds_once, init_thresholds);

	ThreadBuffers *tb= g_tls_buffers;
	if(!tb) {
		tb= (ThreadBuffers *)pthread_getspecific(g_tls_key);
	}

	if(!tb) {
		/* First call on this thread: allocate the struct and register it. */
		tb= calloc(1, sizeof(ThreadBuffers));
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

ThreadBuf *wiki_thread_buf_acquire_scratch_from_data(const char *s, size_t len) {
    /* 1. Get the next available scratch buffer from the pool */
    ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();

    /* 2. Ensure it has enough capacity for the data + null terminator */
    wiki_thread_buf_reserve(scratch, len);

    /* 3. Copy the data if provided */
    if (len > 0 && s != NULL) {
        memcpy(scratch->buf, s, len);
    }
    
    scratch->buf[len] = '\0';
    scratch->len = len;

    return scratch;
}

ThreadBuf *wiki_thread_buf_acquire_scratch(void) {
	ThreadBuffers *tb= wiki_thread_buf_get();

	for(size_t i= 0; i < tb->scratch_count; i++) {
		if(!tb->scratch_in_use[i]) {
			tb->scratch_in_use[i]= true;
			tb->scratch_pool[i]->len= 0;
			return tb->scratch_pool[i];
		}
	}

	if(tb->scratch_count == tb->scratch_cap) {
		size_t new_cap= tb->scratch_cap ? tb->scratch_cap * 2 : INITIAL_SCRATCH_POOL_CAP;
		ThreadBuf **new_pool= realloc(tb->scratch_pool, new_cap * sizeof(ThreadBuf *));
		bool *new_in_use= realloc(tb->scratch_in_use, new_cap * sizeof(bool));
		assert(new_pool && new_in_use);
		tb->scratch_pool= new_pool;
		tb->scratch_in_use= new_in_use;
		for(size_t i= tb->scratch_cap; i < new_cap; i++) {
			tb->scratch_pool[i]= NULL;
			tb->scratch_in_use[i]= false;
		}
		tb->scratch_cap= new_cap;
	}

	size_t idx= tb->scratch_count++;
	tb->scratch_pool[idx]= malloc(sizeof(ThreadBuf));
	assert(tb->scratch_pool[idx]);
	init_thread_buf(tb->scratch_pool[idx], g_scratch_shrink_bytes, g_scratch_target_bytes);
	tb->scratch_in_use[idx]= true;
	return tb->scratch_pool[idx];
}

void wiki_thread_buf_release_scratch(ThreadBuf *scratch) {
	if(!scratch) return;

	ThreadBuffers *tb= wiki_thread_buf_get();
	for(size_t i= 0; i < tb->scratch_count; i++) {
		if(tb->scratch_pool[i] == scratch) {
			tb->scratch_in_use[i]= false;
			tb->scratch_pool[i]->len= 0;
			return;
		}
	}

	assert(!"wiki_thread_buf_release_scratch called with non-pooled buffer");
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

