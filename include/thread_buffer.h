/*
 * thread_buffer.h — Per-thread reusable buffers for wiki_parse().
 *
 * Each OS thread gets one dedicated main buffer plus a pool of scratch
 * buffers that callers lease temporarily:
 *
 *   main    — used to hold the str_tidy() copy of the raw wikitext input,
 *             avoiding a per-call heap allocation for that temporary copy.
 *   scratch — acquired on demand by parser/helper code so nested functions
 *             do not step on each other's temporary workspace.
 *
 * Buffers are allocated on first use per thread and freed either when the
 * thread exits (via the pthread TLS destructor) or when
 * wiki_thread_buf_finalize_all() is called (typically at library shutdown).
 *
 * Shrink/grow policy
 * ------------------
 * wiki_thread_buf_reserve(tb, need) is the ONLY function that resizes a
 * ThreadBuf.  It applies a unified grow-or-shrink policy:
 *
 *   Shrink: tb->cap > tb->shrink_size AND need < tb->target_size
 *           → reallocate at exactly tb->target_size (64-byte aligned).
 *   Grow:   tb->cap < need + 1
 *           → if need > shrink_size: allocate exactly (need + 1)
 *           → if need <= shrink_size: double until large enough.
 *           → All heap allocations are 64-byte aligned for SIMD.
 *   No-op:  otherwise.
 *
 * Each ThreadBuf stores its own shrink_size and target_size, set once at
 * initialisation from the env vars below.  No other code path makes resize
 * decisions.
 *
 * Environment variables
 * ---------------------
 *   TOKENIZER_THREAD_BUFFER_MAIN_SHRINK_SIZE_MB    (default 10)
 *   TOKENIZER_THREAD_BUFFER_MAIN_TARGET_SIZE_MB    (default  5)
 *   TOKENIZER_THREAD_BUFFER_SCRATCH_SHRINK_SIZE_MB (default 10)
 *   TOKENIZER_THREAD_BUFFER_SCRATCH_TARGET_SIZE_MB (default  1)
 */
#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdalign.h>

#if defined(_MSC_VER)
#define THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define THREAD_LOCAL __thread
#else
#define THREAD_LOCAL _Thread_local
#endif

/* ── A single resizable buffer slot ─────────────────────────────────────── */
/*
 * Each ThreadBuf carries its own resize policy so that wiki_thread_buf_reserve()
 * is the single place responsible for all grow/shrink decisions.
 *
 *   shrink_size  — if cap exceeds this threshold AND the requested need
 *                  is less than target_size, the buffer is shrunk down.
 *   target_size  — floor allocation size and the target after a shrink.
 *
 * Both fields are set once at thread-buffer initialisation from the global
 * threshold values (env vars or defaults) and never change afterward.
 *
 * This struct is 64-byte aligned and supports Small String Optimization (SSO).
 * tb->buf points to tb->inline_data for small strings and to heap for large ones.
 */
typedef struct {
    char  *buf;         /* Active pointer: points to inline_data or heap */
    size_t len;         /* bytes of content currently stored (excl. '\0') */
    size_t cap;         /* allocated bytes */
    size_t shrink_size; /* shrink when cap > this AND need < target_size  */
    size_t target_size; /* minimum allocation and post-shrink target size */

    /* SSO / Alignment metadata */
    bool   is_on_heap;  /* True if buf points to heap; false if inline_data */
    char   inline_data[31]; /* Internal storage to avoid heap for small tokens */
} __attribute__((aligned(64))) ThreadBuf;

/* ── The pair of buffers owned by one thread ─────────────────────────────── */

typedef struct {
    ThreadBuf **scratch_pool;
    bool      *scratch_in_use;
    size_t     scratch_count;
    size_t     scratch_cap;
    ThreadBuf  main;
    bool       finalized; /* set by finalize_all; cleared on re-init */
} ThreadBuffers;

/**
 * Return the calling thread's ThreadBuffers, allocating them on first call.
 *
 * This function only handles TLS lookup and initialisation.  Callers that
 * need to resize a buffer must call wiki_thread_buf_reserve() afterward.
 *
 * The returned pointer is owned by TLS and remains valid until:
 *   - the thread exits, or
 *   - wiki_thread_buf_finalize_all() is called and the thread subsequently
 *     calls wiki_thread_buf_get() again (which re-initialises the buffers).
 */
ThreadBuffers *wiki_thread_buf_get(void);

/**
 * Abort the process if any scratch buffers are still leased on the calling
 * thread. If ignore_tb points at one pooled scratch buffer, it is excluded
 * from the check.
 */
void wiki_thread_buf_assert_no_leased_scratch(const char *context,
                                              const ThreadBuf *ignore_tb);


/** 
 * Acquire, resize and copy a scratch buffer. 
 * This is the preferred way to lease a buffer for a known string.
 */
ThreadBuf *wiki_thread_buf_acquire_scratch_from_data(const char *s, size_t len);

/** 
 * Acquire a scratch buffer. 
 * Use this when you need a workspace but don't have the data yet.
 */
ThreadBuf *wiki_thread_buf_acquire_scratch(void);

/** Release a previously acquired scratch buffer. */
void wiki_thread_buf_release_scratch(ThreadBuf *tb);

/**
 * The single authority for all buffer resize decisions.
 *
 * Given a requested byte count `need`, this function applies the buffer's
 * own shrink/grow policy (stored in tb->shrink_size and tb->target_size):
 *
 *   SSO:    If (need + 1) fits in inline_data, no heap allocation is made.
 *   Shrink: if tb->cap > tb->shrink_size AND need < tb->target_size
 *           → free and reallocate at exactly tb->target_size.
 *   Grow:   if tb->cap < need + 1
 *           → if need > shrink_size: allocate exactly need + 1 (aligned).
 *           → otherwise: use a doubling strategy (aligned).
 *
 * All heap allocations are guaranteed 64-byte aligned for SIMD operations.
 * Callers never realloc a ThreadBuf directly; they always go through here.
 */
void wiki_thread_buf_reserve(ThreadBuf *tb, size_t need);

/**
 * Copy s (len bytes) into tb->buf, reserving space first if needed.
 * tb->len is updated to len and tb->buf[len] is set to '\0'.
 * This is the only function other than wiki_thread_buf_reserve that may grow
 * a ThreadBuf; it does so by calling wiki_thread_buf_reserve internally.
 */
void wiki_thread_buf_set(ThreadBuf *tb, const char *s, size_t len);

/**
 * Free the inner buffers of every thread that has called wiki_thread_buf_get().
 *
 * Call this once at library shutdown (after all parse calls have completed).
 * The ThreadBuffers struct for each thread is NOT freed here — it will be
 * freed by the TLS destructor when each thread eventually exits.  This avoids
 * any cross-thread free of memory the destructor still holds a reference to.
 *
 * After this call, the next wiki_thread_buf_get() in any thread will
 * transparently re-allocate fresh buffers.
 */
void wiki_thread_buf_finalize_all(void);