## Hotspot Summary (pre-epoch-fix perf run)

Summed leaf cycles from `perf.folded`:

| Cycles | Hotspot |
|---|---|
| ~4.0 B | `parse_braces → pcre2_match_8` (PCRE matching time) |
| ~1.7 B | `token_free_graph` recursion (**fixed** by epoch marking) |
| ~1.0 B | `parse_comment_and_ext → pcre2_match_8` |
| ~0.9 B | `__memmove_avx512_unaligned_erms` (memcpy in output buffers) |
| ~0.5 B | `parse_magic_links → pcre2_match_8` |
| ~0.2 B | `parse_hr_and_double_underscore → pcre2_match_8` |
| ~0.1 B | `__strcmp_evex` in `parse_comment_and_ext` |
| ~0.1 B | `_int_malloc` / `_int_free` (per-call heap churn) |
| ~0.06 B | `pthread_once` inside `wiki_thread_buf_acquire_scratch` |
| ~0.06 B | `wiki_thread_buf_set` inside `parse_links` |
| ~0.06 B | `__printf_buffer` (`snprintf`) in `parse_magic_links` / `parse_comment_and_ext` |
| ~0.06 B | `realloc` in `pattern_append` (inside `parse_hr_and_double_underscore`) |
| ~0.11 B | `pcre2_match_data_create_8 → malloc` (per-call, `braces` + `hr_and_dunder`) |

---

## Opportunity 5 — Arena allocator for tokens (HIGH IMPACT, larger refactor)

**Problem:** Every `token_new` calls `calloc(1, sizeof(Token))` + `malloc(CHILD_INIT_CAP *
sizeof(Child))`. For a Wikipedia article producing thousands of tokens, this means thousands of
independent heap allocations tracked by glibc, causing cache-thrash during both allocation and
freeing. The flame graph shows continuous `_int_malloc` / `_int_free` contribution.

**Fix:** Introduce a per-parse bump allocator (arena):
- On `wiki_parse()` entry, acquire a large block (e.g. 2 MB) from a thread-local arena.
- `token_new()` bumps the arena pointer — no `malloc`, no free-list lookup.
- Children array starts embedded in the `Token` struct (small SSO) so most tokens need zero extra
  allocation.
- `token_free()` becomes a single arena reset — no per-token free, no epoch field needed.

This eliminates the `_int_malloc` / `_int_free` contributions entirely and eliminates the
`token_free_graph` cost altogether. Estimated impact: further 0.3–0.5 s reduction.

---

## Opportunity 6 — `__strcmp_evex` in `parse_comment_and_ext` (LOW)

~111 M cycles of `strcmp` visible in postprocess chains inside `parse_comment_and_ext`. This is
likely the tag-name comparison in `is_ext_tag()` looping over `cfg->ext_tags` with `strcmp`.

**Fix:** Pre-sort `cfg->ext_tags` at config-load time and use `bsearch`, or build a small hash
table of ext-tag names keyed by first character, reducing comparisons from O(n) to O(1).

---

## Opportunity 7 — Output buffer initial capacity (LOW)

Many parsers do:
```c
size_t out_cap = tb->len * 2 + 64;
```
For small articles this over-allocates; for large ones with many replacements the buffer grows
past `2×`. Profiling shows repeated `__memmove_avx512_unaligned_erms` in output buffer copies.

**Fix:** For parsers that replace sentinels with shorter strings (most do), start at `tb->len + 64`
rather than `tb->len * 2`. For parsers that expand (e.g. `parse_braces` which can add sentinel
strings), keep `2×` or adjust to `1.25×` to reduce memcpy cascade.
