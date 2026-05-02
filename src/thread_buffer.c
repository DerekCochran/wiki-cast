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

/* ── Default thresholds (in megabytes) ───────────────────────────────────── */

#define DEFAULT_MAIN_SHRINK_MB 10
#define DEFAULT_MAIN_TARGET_MB 5
#define DEFAULT_SCRATCH_SHRINK_MB 10
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

#if defined(_MSC_VER)
#define THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define THREAD_LOCAL __thread
#else
#define THREAD_LOCAL _Thread_local
#endif

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
		free_thread_buf(&tb->main);
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
	tb->shrink_size= shrink_size;
	tb->target_size= target_size;
	tb->buf= malloc(target_size);
	assert(tb->buf);
	tb->cap= target_size;
	tb->len= 0;
}

static void free_thread_buf(ThreadBuf *tb) {
	if(!tb) return;
	free(tb->buf);
	tb->buf= NULL;
	tb->cap= 0;
	tb->len= 0;
}

static void free_scratch_pool(ThreadBuffers *tb) {
	if(!tb) return;
	for(size_t i= 0; i < tb->scratch_count; i++) {
		free_thread_buf(&tb->scratch_pool[i]);
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
	init_thread_buf(&tb->main, g_main_shrink_bytes, g_main_target_bytes);
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
		ThreadBuf *scratch= &tb->scratch_pool[i];
		if(!tb->scratch_in_use[i] || scratch == ignore_tb) {
			continue;
		}

		leased_count++;
		leased_cap+= scratch->cap;
		leased_len+= scratch->len;

		if(used < sizeof(details)) {
			int wrote= snprintf(details + used, sizeof(details) - used,
													"%s#%zu(cap=%zu,len=%zu)",
													used == 0 ? "" : ", ",
													i, scratch->cap, scratch->len);
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
	if(tb->cap > tb->shrink_size && need < tb->target_size) {
		/* Shrink to target; sufficient for need because need < target_size. */
		size_t old_cap= tb->cap;
		free(tb->buf);
		tb->buf= malloc(tb->target_size);
		assert(tb->buf);
		tb->cap= tb->target_size;
		log_trace("thread_buffer shrink: old_cap=%zu target_size=%zu shrink_size=%zu need=%zu new_cap=%zu",
							old_cap, tb->target_size, tb->shrink_size, need, tb->cap);
		return;
	}

	if(tb->cap < need + 1) {
		size_t old_cap= tb->cap;
		size_t new_cap= tb->cap ? tb->cap : tb->target_size;
		while(new_cap < need + 1) {
			new_cap+= 1024 * 1024;
		}
		tb->buf= realloc(tb->buf, new_cap);
		assert(tb->buf);
		tb->cap= new_cap;
		log_trace("thread_buffer grow: old_cap=%zu target_size=%zu shrink_size=%zu need=%zu new_cap=%zu",
							old_cap, tb->target_size, tb->shrink_size, need, tb->cap);
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

ThreadBuf *wiki_thread_buf_acquire_scratch(void) {
	ThreadBuffers *tb= wiki_thread_buf_get();

	for(size_t i= 0; i < tb->scratch_count; i++) {
		if(!tb->scratch_in_use[i]) {
			tb->scratch_in_use[i]= true;
			tb->scratch_pool[i].len= 0;
			return &tb->scratch_pool[i];
		}
	}

	if(tb->scratch_count == tb->scratch_cap) {
		size_t new_cap= tb->scratch_cap ? tb->scratch_cap * 2 : INITIAL_SCRATCH_POOL_CAP;
		ThreadBuf *new_pool= realloc(tb->scratch_pool, new_cap * sizeof(ThreadBuf));
		bool *new_in_use= realloc(tb->scratch_in_use, new_cap * sizeof(bool));
		assert(new_pool && new_in_use);
		tb->scratch_pool= new_pool;
		tb->scratch_in_use= new_in_use;
		for(size_t i= tb->scratch_cap; i < new_cap; i++) {
			memset(&tb->scratch_pool[i], 0, sizeof(ThreadBuf));
			tb->scratch_in_use[i]= false;
		}
		tb->scratch_cap= new_cap;
	}

	size_t idx= tb->scratch_count++;
	init_thread_buf(&tb->scratch_pool[idx], g_scratch_shrink_bytes, g_scratch_target_bytes);
	tb->scratch_in_use[idx]= true;
	return &tb->scratch_pool[idx];
}

void wiki_thread_buf_release_scratch(ThreadBuf *scratch) {
	if(!scratch) return;

	ThreadBuffers *tb= wiki_thread_buf_get();
	for(size_t i= 0; i < tb->scratch_count; i++) {
		if(&tb->scratch_pool[i] == scratch) {
			tb->scratch_in_use[i]= false;
			tb->scratch_pool[i].len= 0;
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

		free_thread_buf(&tb->main);
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
