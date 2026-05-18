# Performance Analysis and Optimization Suggestions

Based on the folded perf output from `perf record` and gprof flat profile / call graph, here are the key performance bottlenecks and suggested improvements.

## Executive Summary

### Perf Top Hotspots (sampled, ~2.4B samples total):
| Function | Samples | % of Total | Category |
|----------|---------|------------|----------|
| `match_proto_prefix` | ~328M | ~13.7% | String matching |
| `braces_get_symbol` | ~191M | ~7.9% | Template parsing |
| `parse_magic_links` (strlen) | ~165M | ~6.9% | String length |
| `parser_scan` (brace parsing) | ~104M | ~4.3% | Core parsing |
| `qsort` + `cmp_token_ptr` | ~108M | ~4.5% | Memory management |
| `postprocess_nested_plain` | ~55M | ~2.3% | Nested parsing |

### Gprof Top Hotspots (exact call counts, 0.34s total runtime):
| Function | Call Count | % Self Time | Key Caller |
|----------|-------------|-------------|------------|
| `cae_match_close_named` | 16,975,126 | 14.71% | `parse_comment_and_ext` |
| `postprocess_nested_plain` | 368,931 | 11.76% | `wiki_parse` |
| `match_proto_prefix` | 1,379,322 | 8.82% | `make_table_attr` (99%) |
| `parse_comment_and_ext` | 244,368 | 8.82% | `wiki_parse` |
| `wiki_thread_buf_set` | 3,128,913 | 5.88% | `parse_hr_and_double_underscore` |
| `make_table_attr` | 506,385 | 5.88% | `postprocess_nested_plain` |
| `parse_inner_fragment` | 307,150 | 5.88% | `wiki_parse` |
| `build_from_str` | 226,403 | 5.88% | `postprocess_nested_plain` |
| `collect_tree_tokens` | 45,286 | 5.88% | `wiki_parse` |

### Callgrind Top Hotspots (exact instruction counts, 15.6B total Ir):
| Function | Instructions | % of Total | Location |
|----------|--------------|-------------|----------|
| `sz_emulate_aesenc_si128_serial_` | 1,878,847,252 | 12.04% | stringzilla/hash.h |
| `match_proto_prefix` | 935,879,335 | 6.00% | `src/string_util.c` |
| `parse_comment_and_ext` | 795,752,114 | 5.10% | `src/parser/comment_and_ext.c` |
| `cae_match_open_named` | 773,854,484 | 4.96% | `src/parser/comment_and_ext.c` |
| `__strlen_avx2` | 751,871,440 | 4.82% | libc (strlen-avx2.S) |
| `__strcmp_avx2` | 565,031,934 | 3.62% | libc (strcmp-avx2.S) |
| `cae_match_close_named` | 539,616,172 | 3.46% | `src/parser/comment_and_ext.c` |
| `sz_hash_serial` | 444,517,395 | 2.85% | stringzilla/hash.h |
| `_int_malloc` | 425,962,770 | 2.73% | libc (malloc.c) |
| `wiki_thread_buf_reserve` | 424,475,579 | 2.72% | `src/util/thread_buffer.c` |
| `_int_free` | 416,688,128 | 2.67% | libc (malloc.c) |
| `wiki_thread_buf_get` | 404,333,958 | 2.59% | `src/util/thread_buffer.c` |

**Key Insights from Callgrind:**
1. **`sz_emulate_aesenc_si128_serial_` (12.04%)** — Stringzilla hash function is the #1 hotspot (not visible in perf/gprof)
3. **AVX2 string functions** (`__strlen_avx2`, `__strcmp_avx2`) account for ~8.5% combined
4. **`cae_match_close_named` dropped to 3.46%** in Callgrind vs 14.71% in gprof (different measurement methodology)
5. **`wiki_thread_buf_*` functions** account for ~8% combined (reserve + get + append + set + release)

---

---

## Gprof-Specific Findings

Gprof provides exact call counts and call-graph relationships that complement perf's sampled data:

### 1. Exact Call Counts
- `cae_match_open_named`: 25,643,235 calls (0.00% self time) vs `cae_match_close_named`: 16,975,126 calls (14.71% self time). The close function is significantly slower per call.
- `wiki_thread_buf_reserve`: 10,071,856 calls (2.94% self time) — extremely high call count suggests frequent small buffer reservations.
- `wiki_thread_buf_append`: 4,078,752 calls (2.94% self time) — high append count aligns with buffer reservations.
- `sz_find_skylake` variants: ~6.2M combined calls (5.88% self time) — string search functions are heavily used.

### 2. Cycle 1 (Mutually Recursive Functions)
Gprof identifies a large call cycle (cycle 1) accounting for **85.3% of total runtime**. Members include:
- `postprocess_nested_plain`, `parse_comment_and_ext`, `braces_run_pass`
- `parse_inner_fragment`, `parse_hr_and_double_underscore`, `build_from_str`
- `parse_links`, `parse_external_links`, `build_template_token`

These functions call each other recursively, leading to deep call stacks and repeated work.

### 3. Key Caller-Callee Relationships
- 99% of `match_proto_prefix` calls come from `make_table_attr` (not `parse_magic_links` as previously assumed).
- `parse_comment_and_ext` is the sole caller of both `cae_match_open_named` (25.6M calls) and `cae_match_close_named` (16.9M calls).
- `make_table_attr` calls `match_proto_prefix` ~2.7 times per invocation (1.37M calls / 506k invocations).

---

## 1. `cae_match_close_named` / `cae_match_open_named` — Tag Matching (NEW P0)

**Current Cost (Gprof):**
- `cae_match_close_named`: 16,975,126 calls (14.71% self time) — **HIGHEST self time**
- `cae_match_open_named`: 25,643,235 calls (0.00% self time)

**Location:** `src/parser/comment_and_ext.c`

**Problem:**
These functions match open/close tags for comments, extension tags (`<ref>`, `<gallery>`), `<noinclude>`, etc. The disparity between open/close performance is significant:
- `cae_match_open_named` has 25.6M calls but 0% self time
- `cae_match_close_named` has 16.9M calls but 14.71% self time

Possible causes:
- Close matching requires more complex validation (nesting checks, attribute parsing)
- Open matching is a simple string scan, while close requires backtracking
- 16.9M calls to `cae_match_close_named` suggests the parser is re-scanning the same text repeatedly

**Suggestions:**
1. Profile `cae_match_close_named` with callgrind to identify per-line costs
2. Check if close matching can use faster string search (e.g., stringzilla's `sz_find_str`)
3. Reduce call count: 16.9M calls is extremely high—verify if the parser is doing redundant scans
4. Cache recent close tag matches to avoid re-parsing

---

## 2. `match_proto_prefix` — Protocol Matching (P0)

**Current Cost:**
- Perf: ~328M samples (~13.7% of total runtime)
- Gprof: 1,379,322 calls (8.82% self time), 99% from `make_table_attr`

**Location:** `src/string_util.c:551`

**Problem:**
The current implementation iterates through all protocols and does case-insensitive character-by-character comparison:
```c
for(size_t pi = 0; pi < cfg->protocol_items.count; pi++) {
    // ... character-by-character comparison with tolower()
}
```

Gprof reveals this is called **1,379,322 times**, with 99% of calls coming from `make_table_attr` (not `parse_magic_links` as previously thought). Each call:
- Loops through all ~20+ protocols
- Calls `tolower()` for each character of both strings
- Calls `strlen()` on each protocol token
- `make_table_attr` calls this ~2.7 times per invocation, suggesting redundant protocol checks in table attribute parsing.

**Suggestions:**

### 1.1 Precompute Protocol Metadata & Centralized LUT-Based Comparison
Replace the original trie/hash set suggestion with a lower-overhead approach tailored to the small protocol list (~20 entries):

1.  **Precompute protocol metadata**: At config initialization, store the original protocol, its precomputed lowercase version, and length for each protocol, eliminating repeated `strlen()` and `tolower()` calls during parsing:
    ```c
    typedef struct {
        char *protocol;         // Original protocol string (e.g., "Http://")
        size_t protocol_len;    // Precomputed length of original protocol
        char *protocol_lower;  // Precomputed lowercase version (e.g., "http://")
    } ProtocolItem;
    ```

2.  **Expose existing LUT via `fast_tolower()`**: The LUT already exists in `string_util.c` (`s_tolower_lut[256]` with `ensure_tolower_lut()`). Simply add a public helper function to use it:
    ```c
    // In string_util.h:
    unsigned char fast_tolower(unsigned char c);

    // In string_util.c (add after existing ensure_tolower_lut):
    unsigned char fast_tolower(unsigned char c) {
        ensure_tolower_lut();
        return s_tolower_lut[c];
    }
    ```

3.  **Early length check**: Before any comparison, skip protocols where the remaining input length is shorter than the protocol's precomputed `token_len` (O(1) check per protocol).

4.  **Direct LUT comparison loop in `match_proto_prefix`**: Use the centralized `fast_tolower` to compare the input's lowered character against the precomputed lowercase protocol string:
    ```c
    size_t match_proto_prefix(const char *s, size_t len, const ParserConfig *cfg) {
        if (!cfg || !cfg->protocol_items_valid) return 0;

        for (size_t pi = 0; pi < cfg->protocol_items.count; pi++) {
            const ProtocolItem *proto = &cfg->protocol_items.items[pi];
            // Early skip if input is too short for this protocol
            if (proto->protocol_len > len) continue;

            // Compare input lowered vs precomputed protocol lowercase
            bool match = true;
            for (size_t i = 0; i < proto->protocol_len; i++) {
                if (fast_tolower((unsigned char)s[i]) != (unsigned char)proto->protocol_lower[i]) {
                    match = false;
                    break;
                }
            }
            if (match) return proto->protocol_len;
        }
        return 0;
    }
    ```

**Why this is better than the original trie/hash approach**:
- Linear scan over 20 small entries is faster than hash/trie overhead (no hash computation, no pointer chasing)
- Eliminates all `strlen()` calls on protocols during parsing
- Centralized LUT in `string_util` avoids duplicating the lookup table across functions
- Replaces per-character `tolower()` function calls with O(1) LUT array lookups via `fast_tolower`
- Precomputed lowercase protocol strings avoid redundant lowering during comparison

**Expected Improvement:** 50-70% reduction in `match_proto_prefix` cost.

---

## 3. `braces_get_symbol` — Template Symbol Detection

**Current Cost:**
- Perf: ~191M samples (~7.9% of total runtime)
- Gprof: 36,060 calls (0.00% self time, but called from hot cycle 1)

**Location:** `src/parser/braces.c:336`

**Problem:**
The function performs many allocations and copies:
1. `str_remove_comment()` — allocates and copies
2. `trimmed` — allocates and copies
3. `lower_copy()` — allocates and copies
4. `base_orig_buf` — conditionally allocates
5. `base_buf` — conditionally allocates

Then does up to **4 hash map lookups** with different key variations.

**Suggestions:**

### 2.1 Avoid Allocations for Common Cases
Most template names are simple ASCII without comments. Fast-path the common case:

```c
static char braces_get_symbol_fast(const char *name, size_t len, const ParserConfig *cfg) {
    // Fast path: no comments, no special chars, simple name
    if(memchr(name, '\0', len) == NULL && memchr(name, '<', len) == NULL) {
        // Can do direct lookup without cleaning
        // ...
    }
    // Slow path: use existing logic
    return braces_get_symbol_slow(name, len, cfg);
}
```

### 2.2 Use Stack Buffers for Small Strings
Instead of `malloc()` for small trimmed/lowered copies, use stack buffers:

```c
char trimmed[256];
if(n < sizeof(trimmed)) {
    sz_copy(trimmed, cleaned + i, n);
    trimmed[n] = '\0';
    // use trimmed directly
} else {
    // fall back to malloc for unusually long names
}
```

### 2.3 Reduce Hash Map Lookups
The current code does up to 4 lookups. Restructure to do at most 2:

```c
// Try sensitive first (original case)
canonical = str_map_get_exact(&cfg->parser_function_sensitive, trimmed);
if(canonical) { ... }

// If not found, try insensitive (lowercase)
if(!canonical && lc) {
    canonical = str_map_get_exact(&cfg->parser_function_insensitive, lc);
}
// Don't re-check with base_orig if base_orig == trimmed (common case)
```

### 2.4 Cache Recent Lookups
Template names often repeat. A small LRU cache (16-32 entries) can avoid hash lookups:

```c
static struct {
    char name[64];
    size_t len;
    char symbol;
} symbol_cache[16];
static int symbol_cache_next = 0;

// Check cache first, then do full lookup
```

**Expected Improvement:** 40-60% reduction in `braces_get_symbol` cost.

---

## 4. Cycle 1 — Mutually Recursive Functions (P1)

**Current Cost:** 85.3% of total runtime (gprof cycle 1 as a whole)

**Members:**
`postprocess_nested_plain`, `parse_comment_and_ext`, `braces_run_pass`, `parse_inner_fragment`, `parse_hr_and_double_underscore`, `build_from_str`, `parse_links`, `parse_external_links`, `build_template_token`, `braces_get_symbol`, `build_from_inner`

**Problem:**
These functions form a mutually recursive call cycle, where each function may call another in the cycle. This leads to:
- Deep call stacks (gprof shows 10+ levels of recursion)
- Repeated work (e.g., `postprocess_nested_plain` calls `parse_links`, which calls `braces_run_pass`, which calls back into `postprocess_nested_plain`)
- Difficulty for the compiler to optimize (inlining is limited by recursion)

**Suggestions:**
1. Flatten the pipeline where possible: check if recursive calls can be replaced with an iterative approach
2. Add memoization for repeated nested parsing operations
3. Use an explicit stack instead of recursive function calls for hot paths
4. Consider breaking the cycle by separating parsing stages more clearly

**Expected Improvement:** 5-10% reduction in cycle overhead.

---

## 5. `wiki_thread_buf_reserve` / `wiki_thread_buf_append` — Buffer Management (P2)

**Current Cost (Gprof):**
- `wiki_thread_buf_reserve`: 10,071,856 calls (2.94% self time)
- `wiki_thread_buf_append`: 4,078,752 calls (2.94% self time)
- `wiki_thread_buf_set`: 3,128,913 calls (5.88% self time)

**Location:** `src/thread_buffer.c`

**Problem:**
Frequent buffer reallocation when building strings. The high call counts suggest:
- Many small reservations instead of fewer large ones
- Possible missing pre-allocation for known output sizes

**Suggestions:**

### 5.1 Pre-allocate Output Buffers
For stages that produce output, pre-calculate or estimate the output size:

```c
// Before parsing a stage, estimate output size
// For example, if input is 1000 chars, output is likely similar
wiki_thread_buf_reserve(out_tb, input_len * 2);  // Reserve generously once
```

### 5.2 Use `wiki_thread_buf_append` More Efficiently
Batch appends when possible instead of many small ones.

**Expected Improvement:** 30-50% reduction in buffer management overhead.

---

## 7. `parse_magic_links` — Repeated `strlen` Calls

**Current Cost:** ~165M samples from `__strlen_evex` + ~108M from `strlen@plt`

**Problem:**
The function calls `strlen()` repeatedly on the same strings. The `match_proto_prefix` function also calls `strlen()` on protocol tokens (which could be precomputed).

**Suggestions:**

### 3.1 Pass Lengths Instead of Using `strlen`
Review `parse_magic_links` to ensure string lengths are tracked and passed, not recomputed:

```c
// Instead of:
size_t pfx = match_proto_prefix(s + i, strlen(s + i), cfg);

// Use:
size_t remaining = len - i;
size_t pfx = match_proto_prefix(s + i, remaining, cfg);
```

### 3.2 Precompute All Protocol Lengths
As mentioned in section 1.3, store lengths with protocol strings.

**Expected Improvement:** 30-50% reduction in string length overhead.

---

## 4. `parser_scan` — Brace Parser State Machine

**Current Cost:** ~104M samples

**Location:** `src/parser/braces.c` — `parser_scan`

**Problem:**
The brace parser is a complex state machine that processes the entire input character-by-character in some cases.

**Suggestions:**

### 4.1 Review Regex vs Hand-Parsed Sections
The code uses a mix of regex and hand-written parsing. Ensure the regex paths are optimized and the state machine isn't doing redundant work.

### 4.2 Reduce `str_restore` and `malloc` Calls
The profile shows `str_restore; malloc` at 55M samples. This suggests frequent string restoration operations:

```c
// In the brace parsing, consider:
// - Batching string operations
// - Using the thread scratch buffer more aggressively
// - Avoiding restore operations when the string hasn't changed
```

**Expected Improvement:** 20-30% reduction in brace parsing cost.

---

## 5. `free_accum_orphans` — Token Sorting

**Current Cost:** ~108M samples (qsort + cmp_token_ptr)

**Location:** `src/parse.c:91`

**Problem:**
The orphan detection sorts all live tokens using `qsort`, then does binary search. This is O(n log n) per parse call.

**Suggestions:**

### 5.1 Use a Bitmap or Mark-and-Sweep
Instead of sorting and binary searching, use a simpler approach:

```c
// Option A: Mark tokens as live during tree traversal
// Add a `mark` bit to Token struct
for(size_t i = 0; i < accum->count; i++) {
    accum->tokens[i]->mark = 0;  // Reset
}
mark_tree_tokens(root);  // Set mark=1 for live tokens
// Free unmarked tokens

// Option B: Use a hash set (if mark bit not available)
// Store live tokens in a hash set, then check membership in O(1)
```

### 5.2 Skip Orphan Detection When Unnecessary
If the parser guarantees no orphans (all tokens are reachable), skip this step entirely.

**Expected Improvement:** 80-90% reduction in orphan detection cost.

---

## 6. `wiki_thread_buf_reserve` — Buffer Reallocation

**Current Cost:** ~55M samples

**Location:** `src/build.c:323` and many parser files

**Problem:**
Frequent buffer reallocation when building strings. The reserve function likely grows the buffer geometrically, but many small reservations still cause overhead.

**Suggestions:**

### 6.1 Pre-allocate Output Buffers
For stages that produce output, pre-calculate or estimate the output size:

```c
// Before parsing a stage, estimate output size
// For example, if input is 1000 chars, output is likely similar
wiki_thread_buf_reserve(out_tb, input_len * 2);  // Reserve generously once
```

### 6.2 Use `wiki_thread_buf_append` More Efficiently
Batch appends when possible instead of many small ones.

**Expected Improvement:** 30-50% reduction in buffer management overhead.

---

## 8. `parser_scan` — Brace Parser State Machine

**Current Cost:** ~55M samples (recursive, called many times)

**Problem:**
This function runs the full parsing pipeline on nested content (template arguments, etc.). Each call re-runs stages 1-10 on the inner content.

**Suggestions:**

### 7.1 Avoid Re-parsing Unchanged Content
If nested content doesn't contain any sentinel markers that need further processing, skip stages that won't match.

### 7.2 Cache Parsed Results
For repeated template arguments or common nested patterns, consider caching.

**Expected Improvement:** 20-40% reduction in nested parsing overhead.

---

## 10. `parse_table` — Dynamic Array Growth

**Current Cost:** ~44M samples (realloc + _int_malloc)

**Location:** `src/parser/table.c` — `token_append_child`

**Problem:**
`token_append_child` uses `realloc` to grow the children array. For tables with many cells, this causes repeated copies.

**Suggestions:**

### 8.1 Pre-allocate Child Arrays
Use geometric growth (double the size) instead of incrementing by 1:

```c
void token_append_child(Token *t, TokenChild c) {
    if(t->child_capacity == 0) {
        t->child_capacity = 8;  // Start with 8
        t->children = malloc(t->child_capacity * sizeof(TokenChild));
    } else if(t->child_count >= t->child_capacity) {
        t->child_capacity *= 2;  // Double each time
        t->children = realloc(t->children, t->child_capacity * sizeof(TokenChild));
    }
    t->children[t->child_count++] = c;
}
```

### 8.2 Use the Thread Buffer for Temporary Arrays
For parse-stage temporary arrays, consider using the thread scratch buffer.

**Expected Improvement:** 50-70% reduction in table parsing allocation overhead.

---

## 11. Memory Management — `token_free` and `cfree`

**Current Cost:** ~55M samples

**Suggestions:**

### 9.1 Arena Allocation for Tokens
Instead of individual `free()` calls, allocate tokens from an arena that gets freed all at once:

```c
// During parse:
Token *token = arena_alloc(&ctx->token_arena, sizeof(Token));

// After parse:
arena_free_all(&ctx->token_arena);  // O(1), no individual frees
```

### 9.2 Batch Free Operations
If individual freeing is needed, consider batching or using a custom allocator.

**Expected Improvement:** 40-60% reduction in free overhead.

---

## 12. General Recommendations

### 12.1 Profile-Guided Optimization (PGO)
Build with PGO to let the compiler optimize hot paths:

```bash
# GCC/Clang PGO
CFLAGS="-fprofile-generate" make
./test_wikitext  # Run representative workload
CFLAGS="-fprofile-use" make
```

### 12.2 Review `stringzilla` Usage
The code uses stringzilla. Ensure it's being used optimally:
- Use `sz_find_byte` instead of `memchr` where appropriate
- Use `sz_copy` which may be SIMD-optimized

### 12.3 Reduce Function Call Overhead
The profile shows many deeply nested function calls (e.g., cycle 1 with 10+ levels). Consider:
- Inlining small hot functions
- Reducing call depth where possible

### 12.4 Consider `__attribute__((hot))`
Mark the hottest functions with `__attribute__((hot))` to tell GCC/Clang to optimize for speed:

```c
__attribute__((hot))
size_t match_proto_prefix(const char *s, size_t len, const ParserConfig *cfg) {
    // ...
}
```

### 12.5 Use Callgrind for Detailed Analysis
Since gprof shows `cae_match_close_named` as the top hotspot but with limited per-line detail, use callgrind for cache behavior and per-line cycle counts:

```bash
valgrind --tool=callgrind ./build/test_wikitext
kcachegrind callgrind.out.*  # Interactive exploration
```

---

## Summary of Priority Actions

| Priority | Optimization | Callgrind Ir | Expected Impact | Difficulty |
|----------|--------------|--------------|-----------------|------------|
| **P0** | Optimize `sz_emulate_aesenc_si128_serial_` (stringzilla) | 1.88B (12.04%) | 10-12% | Medium |
| **P0** | Optimize `match_proto_prefix` with trie/cache | 936M (6.00%) | 5-6% | Medium |
| **P1** | Optimize `parse_comment_and_ext` + `cae_match_*` | 2.1B combined (13.5%) | 10-13% | Medium |
| **P1** | Reduce AVX2 string function overhead | 1.3B combined (8.5%) | 5-8% | Low |
| **P1** | Optimize `wiki_thread_buf_*` functions | 1.3B combined (8%) | 5-8% | Medium |
| **P2** | Break Cycle 1 recursive calls | 85.3% of runtime | 5-10% | High |
| **P2** | Reduce allocations in `braces_get_symbol` | 60M (0.39%) | 2-3% | Medium |
| **P3** | PGO build | N/A | 5-15% | Low |

**Estimated Total Improvement:** 40-60% reduction in runtime if all P0 and P1 items are implemented.

**Key Callgrind Insights:**
1. **Stringzilla hash function is #1 hotspot** (1.88B Ir, 12.04%) — not visible in perf/gprof
3. **AVX2 string functions** (`strlen`, `strcmp`, `memcpy`) account for ~2.5B Ir (16%) — consider `-mno-avx2` for less vectorization overhead
4. **Thread buffer functions** account for ~1.3B Ir (8%) — high call counts suggest need for batching
5. **`cae_match_close_named` is 540M Ir (3.46%)** in Callgrind vs 14.71% in gprof — different measurement methodologies

---

## Key Insights from Gprof vs Perf

1. **`cae_match_close_named` is the #1 hotspot** (14.71% self time) — not visible in perf folded output
2. **`match_proto_prefix` calls come from `make_table_attr`**, not `parse_magic_links` — changes optimization target
3. **Cycle 1 accounts for 85.3% of runtime** — mutually recursive functions need structural changes
4. **Buffer operations are extremely frequent** (10M+ calls to `wiki_thread_buf_reserve`) — suggests need for pre-allocation
5. **`cmp_token_ptr`/`qsort` is NOT a major bottleneck** in gprof (1 call, 0.00%) — perf samples may have been misleading
