Evaluate each of the attached files for performance enhancements using Stringzilla. 

## Protocol
1. Read the **ENTIRE** first file into the context window 500 lines at a time.  Do not skip any lines.  Do not start analyzing until you have read the entire file.
2. Look at each method, one at a time
3. Look for optimization opportunities:
- Replacing string operations with stringzilla
- Reducing unnecessary allocation of memory or copying of data
- The functionality MUST remain the same.
- Take this step-by-step, and do not skip any method or file.
- Look for loops that could be optimized by using stringzilla functions.
- Look for UTF8 cases that could be optimized by using the UTF8 functions in stringzilla.
- Look for any other opportunities to optimize the code while maintaining the same functionality.
- Look at the sz_string_view_t type and see if it can be used to optimize any string operations instead of creating and copying memory
4. DO NOT JUST LOOK AT HOT SPOTS!  Look at the entire codebase for optimization opportunities.  There are opportunities for optimization in every file and method, even if they are not immediately obvious.  You must look carefully and consider all possibilities for enhancement.
5. Build and test the application using the below:
```bash
cd /home/djc/git/wikiparser-node-c-tokenizer
cmake --build build -j"$(nproc)"
cd /home/djc/git/wikiparser-node-c-tokenizer/build
ctest --output-on-failure
```
6. Move on to the next file and repeat the process until all files have been evaluated.
## Patterns 
These are just some of the patterns to look for when analyzing the code.  There may be other opportunities for optimization that are not listed here, so you must look carefully at each method and consider all possibilities for enhancement.
- Do the work on a caller owned buffer instead of allocating new memory.
- Replace libc string functions with Stringzilla equivalents (e.g. sz_find_byte instead of strchr, sz_copy instead of memcpy).
- Use a 256-entry byte lookup table plus `sz_lookup` for fast, branchless lowercasing/uppercasing of ASCII/byte sequences; this avoids per-byte `tolower` loops and is already implemented in `src/string_util.c`.
## Rules
1. You must analyze EVERY file
2. You must analyze EVERY method in each file
3. You must not skip any method or file
4. You must not change the functionality of the code
5. You must NOT just concentrate on hot spots. You must look at the entire codebase for optimization opportunities.
7. There are opportunities for optimization in every file and method, even if they are not immediately obvious. You must look carefully and consider all possibilities for enhancement.
8. After completing the tasks, verify that every method has been evaluated AND updated.
9. Do not update the file until you have completed the analysis and double checked it.
10. When completed, update one file at a time.  The context window is not big enough to update all files at once, so you must update them sequentially.  Do not skip any files or methods during the update process.
## Stringzilla
StringZilla is the GodZilla of string libraries, using SIMD and SWAR to accelerate binary and UTF-8 string operations on modern CPUs and GPUs. It delivers up to 10x higher CPU throughput in C, C++, Rust, Python, and other languages, and can be 100x faster than existing GPU kernels, covering a broad range of functionality. It accelerates exact and fuzzy string matching, hashing, edit distance computations, sorting, provides allocation-free lazily-evaluated smart-iterators, and even random-string generators.
By design, StringZilla has a couple of notable differences from LibC:
all strings are expected to have a length, and are not necessarily null-terminated.
every operations has a reverse order counterpart.
That way sz_find and sz_rfind are similar to strstr and strrstr in LibC. Similarly, sz_find_byte and sz_rfind_byte replace memchr and memrchr. The sz_find_byteset maps to strspn and strcspn, while sz_rfind_byteset has no sibling in LibC.

| LibC Functionality | StringZilla Equivalents |
|---|---|
| `memchr(haystack, needle, haystack_length)`, `strchr` | `sz_find_byte(haystack, haystack_length, needle)` |
| `memrchr(haystack, needle, haystack_length)` | `sz_rfind_byte(haystack, haystack_length, needle)` |
| `memcmp`, `strcmp` | `sz_order`, `sz_equal` |
| `strlen(haystack)` | `sz_find_byte(haystack, haystack_length, needle)` |
| `strcspn(haystack, reject)` | `sz_find_byteset(haystack, haystack_length, reject_bitset)` |
| `strspn(haystack, accept)` | `sz_find_byte_not_from(haystack, haystack_length, accept, accept_length)` |
| `memmem(haystack, haystack_length, needle, needle_length)`, `strstr` | `sz_find(haystack, haystack_length, needle, needle_length)` |
| `memcpy(destination, source, destination_length)` | `sz_copy(destination, source, destination_length)` |
| `memmove(destination, source, destination_length)` | `sz_move(destination, source, destination_length)` |
| `memset(destination, value, destination_length)` | `sz_fill(destination, destination_length, value)` |


### compare.h
1. sz_equal: Checks if two strings are equal up to a specified length.
   - Parameters: `a` (first string), `b` (second string), `length` (bytes to compare).
   - Return: `true` if equal, `false` otherwise.
   - Example:
     ```c
     sz_equal("hello", "hello", 5); // returns true
     sz_equal("hello", "world", 5); // returns false
     ```
2. sz_order: Compares two strings lexicographically, returning their order (less than, equal, greater than).
   - Parameters: `a` and `b` (strings), `a_length` and `b_length` (their lengths).
   - Return: `LESS`, `EQUAL`, or `GREATER`.
   - Example:
     ```c
     sz_order("apple", 5, "banana", 6); // returns LESS
     sz_order("grape", 5, "grape", 5); // returns EQUAL
     sz_order("zebra", 5, "apple", 5); // returns GREATER
     ```
### find.h
1. `sz_find_byte`
- Description: Finds the first occurrence of a single-byte needle in a haystack string.
- Parameters:
  - `haystack`: Pointer to the haystack string.
  - `h_length`: Length of the haystack.
  - `needle`: Pointer to the needle (a single byte).
- Return Value: Pointer to the first matching byte, or `NULL` if not found.
- Example: 
  ```c
  char haystack[] = "hello";
  char needle = 'l';
  sz_find_byte(haystack, sizeof(haystack), &needle);
  // Returns pointer to the first 'l' in haystack.
  ```
2. `sz_rfind_byte`
- Description: Finds the last occurrence of a single-byte needle in a haystack string.
- Parameters:
  - Same as `sz_find_byte`.
- Return Value: Pointer to the last matching byte, or `NULL` if not found.
- Example: 
  ```c
  char haystack[] = "hello";
  char needle = 'l';
  sz_rfind_byte(haystack, sizeof(haystack), &needle);
  // Returns pointer to the last 'l' in haystack.
  ```
3. `sz_find`
- Description: Finds the first occurrence of a substring (needle) in a haystack string.
- Parameters:
  - `haystack`: Pointer to the haystack string.
  - `h_length`: Length of the haystack.
  - `needle`: Pointer to the needle substring.
  - `n_length`: Length of the needle.
- Return Value: Pointer to the first match, or `NULL` if not found.
- Example: 
  ```c
  char haystack[] = "hello";
  char needle[] = "ll";
  sz_find(haystack, sizeof(haystack), needle, sizeof(needle));
  // Returns pointer to the start of "ll" in haystack.
  ```
4. `sz_rfind`
- Description: Finds the last occurrence of a substring (needle) in a haystack string.
- Parameters:
  - Same as `sz_find`.
- Return Value: Pointer to the last match, or `NULL` if not found.
- Example: 
  ```c
  char haystack[] = "hello";
  char needle[] = "ll";
  sz_rfind(haystack, sizeof(haystack), needle, sizeof(needle));
  // Returns pointer to the start of the last "ll" in haystack.
  ```
5. `sz_find_byteset`
- Description: Finds the first occurrence of any character from a set in a haystack string.
- Parameters:
  - `text`: Pointer to the text string.
  - `length`: Length of the text.
  - `set`: Pointer to the byteset containing the characters to search for.
- Return Value: Pointer to the first matching character, or `NULL` if not found.
- Example: 
  ```c
  char text[] = "hello";
  sz_byteset_t set;
  sz_byteset_init(&set);
  sz_byteset_add(&set, 'l');
  sz_byteset_add(&set, 'o');
  sz_find_byteset(text, sizeof(text), &set);
  // Returns pointer to the first 'l' or 'o' in text.
  ```
6. `sz_rfind_byteset`
- Description: Finds the last occurrence of any character from a set in a haystack string.
- Parameters:
  - Same as `sz_find_byteset`.
- Return Value: Pointer to the last matching character, or `NULL` if not found.
- Example: 
  ```c
  char text[] = "hello";
  sz_byteset_t set;
  sz_byteset_init(&set);
  sz_byteset_add(&set, 'l');
  sz_byteset_add(&set, 'o');
  sz_rfind_byteset(text, sizeof(text), &set);
  // Returns pointer to the last 'l' or 'o' in text.
  ```
7. `sz_find_byte_from`
- Description: Finds the first occurrence of a character from a set in a haystack string (convenience function).
- Parameters:
  - `h`: Pointer to the haystack string.
  - `h_length`: Length of the haystack.
  - `n`: Pointer to the needle characters.
  - `n_length`: Number of characters in the needle.
- Return Value: Pointer to the first matching character, or `NULL` if not found.
- Example: 
  ```c
  char haystack[] = "hello";
  sz_find_byte_from(haystack, sizeof(haystack), "lo", 2);
  // Returns pointer to the first 'l' or 'o' in haystack.
  ```
8. `sz_rfind_byte_not_from`
- Description: Finds the last occurrence of a character not from a set in a haystack string (convenience function).
- Parameters:
  - Same as `sz_find_byte_from`.
- Return Value: Pointer to the last non-matching character, or `NULL` if not found.
- Example: 
  ```c
  char haystack[] = "hello";
  sz_rfind_byte_not_from(haystack, sizeof(haystack), "lo", 2);
  // Returns pointer to the last character in haystack that is not 'l' or 'o'.
  ```
9. `sz_find_delimiter_utf8`
- Description: Finds the first occurrence of a UTF-8 whitespace or punctuation character in a string.
- Parameters:
  - `text`: Pointer to the text string.
  - `length`: Length of the text.
  - `matched_length`: Pointer to store the length of the matched delimiter.
- Return Value: Pointer to the first matching delimiter, or `NULL` if not found.
- Example: 
  ```c
  char text[] = "Hello, World!";
  sz_size_t matched_length;
  sz_find_delimiter_utf8(text, sizeof(text), &matched_length);
  // Returns pointer to the first comma (',') and sets matched_length to 1.
  ```
10. `sz_rfind_delimiter_utf8`
- Description: Finds the last occurrence of a UTF-8 whitespace or punctuation character in a string.
- Parameters:
  - Same as `sz_find_delimiter_utf8`.
- Return Value: Pointer to the last matching delimiter, or `NULL` if not found.
- Example: 
  ```c
  char text[] = "Hello, World!";
  sz_size_t matched_length;
  sz_rfind_delimiter_utf8(text, sizeof(text), &matched_length);
  // Returns pointer to the last comma (',') and sets matched_length to 1.
  ```
### intersect.h
1. Function: `sz_sequence_intersect`
- Purpose: Computes the intersection of two deduplicated string collections using a hash table.
- Parameters:
  - `first_sequence`: First sequence of strings to intersect.
  - `second_sequence`: Second sequence of strings to intersect.
  - `semantics`: Join semantics (e.g., inner strict, left outer).
  - `alloc`: Optional memory allocator for temporary storage.
  - `seed`: Seed for hash table to prevent attacks.
  - `intersection_size`: Number of matching strings.
  - `first_positions`: Positions of matches in the first sequence.
  - `second_positions`: Positions of matches in the second sequence.
- Return: `sz_success_k` on success, `sz_bad_alloc_k` on memory failure.
- Preconditions: Output arrays must be large enough to hold results.
- Example:
  ```c
  sz_status_t status = sz_sequence_intersect(&first_sequence, &second_sequence,
      sz_join_inner_strict_k, NULL, 0,
      &intersection_size, first_positions, second_positions);
  ```
  This finds common strings between two sequences and returns their positions.

2. Enumerations: `sz_sequence_join_semantics_t`
- Join Types:
  - Inner Strict (`sz_join_inner_strict_k`): Returns unique matches; fails if duplicates exist.
  - Inner (`sz_join_inner_k`): Returns all matching pairs, including duplicates.
  - Left Outer (`sz_join_left_outer_k`): Includes all entries from the first sequence, with NULL for no matches in the second.
  - Right Outer (`sz_join_right_outer_k`): Includes all entries from the second sequence, with NULL for no matches in the first.
  - Full Outer (`sz_join_full_outer_k`): Combines all entries from both sequences, including NULLs where there's no match.
  - Cross (`sz_join_cross_k`): Computes the Cartesian product of both sequences.
### memory.h
1. sz_copy
- Description: Copies the contents of one string into another.
- Parameters:
  - `target`: Pointer to the destination buffer.
  - `source`: Pointer to the source buffer.
  - `length`: Number of bytes to copy.
- Example:
```c
char output[2];
sz_copy(output, "hi", 2); // output becomes {'h', 'i'}
```
2. sz_move
- Description: Copies (moves) contents of one string into another, allowing overlapping regions.
- Parameters:
  - `target`: Pointer to the destination buffer.
  - `source`: Pointer to the source buffer.
  - `length`: Number of bytes to move.
- Example:
```c
char buffer[3] = {'a', 'b', 'c'};
sz_move(buffer, buffer + 1, 2); // buffer becomes {'b', 'c', 'c'}
```
3. sz_fill
- Description: Fills a string with a specified value.
- Parameters:
  - `target`: Pointer to the destination buffer.
  - `length`: Number of bytes to fill.
  - `value`: The byte value to use for filling.
- Example:
```c
char buffer[2];
sz_fill(buffer, 2, 'x'); // buffer becomes {'x', 'x'}
```
4. sz_lookup
- Description: Applies a lookup table transformation to a string.
- Parameters:
  - `target`: Pointer to the destination buffer.
  - `length`: Number of bytes in the source string.
  - `source`: Pointer to the source buffer.
  - `lut`: A 256-byte lookup table for character transformations.
- Example:
### sort.h
`sz_sequence_argsort`
- Description: Sorts an immutable sequence of strings lexicographically using QuickSort and returns the permutation 
order.
- Parameters:
  - `sequence`: Pointer to the input string sequence.
  - `alloc`: Optional memory allocator for temporary storage.
  - `order`: Output array containing the permutation indices.
- Example:
  ```c
  sz_sequence_t seq;
  sz_sequence_from_null_terminated_strings(strings, count, &seq);
  sz_sorted_idx_t order[count];
  sz_status_t status = sz_sequence_argsort(&seq, NULL, order);
  ```
`sz_pgrams_sort`
- Description: Sorts a continuous array of unsigned integers in place and returns the permutation order.
- Parameters:
  - `pgrams`: Pointer to the input integer array.
  - `count`: Number of elements in the array.
  - `alloc`: Optional memory allocator for temporary storage.
  - `order`: Output array containing the permutation indices.
- Example:
  ```c
  sz_pgram_t pgrams[] = {42, 17, 99, 8};
  sz_sorted_idx_t order[4];
  sz_status_t status = sz_pgrams_sort(pgrams, 4, NULL, order);
  ```
### utf8_case
1. Function: `sz_utf8_case_fold`
   - Description: Applies Unicode case folding to a UTF-8 string, converting uppercase letters to lowercase and 
handling special expansions defined in the Unicode standard.
   - Parameters:
     - `source`: Pointer to the input UTF-8 string.
     - `source_length`: Number of bytes in the source string.
     - `destination`: Pointer to the buffer where the result will be written.
   - Return: The number of bytes written to the destination buffer.
   - Example:
     ```c
     sz_size_t result = sz_utf8_case_fold("HELLO", 5, destination);
     // Result: "hello" is stored in `destination`, and `result` is 5.
     ```
2. Function: `sz_utf8_case_insensitive_find`
   - Description: Searches for a case-insensitive substring within a UTF-8 haystack string using Unicode case 
folding rules.
   - Parameters:
     - `haystack`: Pointer to the haystack string.
     - `haystack_length`: Number of bytes in the haystack.
     - `needle`: Pointer to the substring to search for.
     - `needle_length`: Number of bytes in the needle.
     - `needle_metadata`: Pointer to a metadata structure (optional, for reuse).
     - `matched_length`: Pointer to store the length of the matched substring.
   - Return: A pointer to the start of the matching substring or `SZ_NULL_CHAR` if not found.
   - Example:
     ```c
     sz_cptr_t result = sz_utf8_case_insensitive_find(haystack, haystack_len, "hello", needle_len, NULL, &match_length);
     // Returns the starting position of "hello" in `haystack` or `SZ_NULL_CHAR`.
     ```
3. Function: `sz_utf8_case_insensitive_order`
   - Description: Compares two UTF-8 strings case-insensitively using Unicode case folding rules.
   - Parameters:
     - `a`: Pointer to the first string.
     - `a_length`: Number of bytes in the first string.
     - `b`: Pointer to the second string.
     - `b_length`: Number of bytes in the second string.
   - Return: One of `sz_less_k`, `sz_equal_k`, or `sz_greater_k` based on the comparison result.
   - Example:
     ```c
     sz_ordering_t result = sz_utf8_case_insensitive_order("Hello", 5, "HELLO", 5);
     // Returns `sz_equal_k`.
     ```
4. Function: `sz_utf8_case_invariant`
   - Description: Checks if a UTF-8 string contains only case-agnostic codepoints (those that do not change under 
case folding and are not part of bicameral scripts).
   - Parameters:
     - `str`: Pointer to the input string.
     - `length`: Number of bytes in the string.
   - Return: `sz_true_k` if all codepoints are case-agnostic, otherwise `sz_false_k`.
   - Example:
     ```c
     sz_bool_t result = sz_utf8_case_invariant("价格：¥1234", haystack_len);
     // Returns `sz_true_k` for CJK text and punctuation.
     ```
Notes:
- All functions assume valid UTF-8 input. Behavior is undefined for invalid UTF-8.
- For `sz_utf8_case_fold`, ensure the destination buffer is sufficiently large (at least `source_length * 3` bytes) to 
handle worst-case expansions.
### utf8_word.h
1. `sz_rune_word_break_property(uint32_t codepoint)`
- Description: Retrieves the TR29 Word_Break property value for a given Unicode codepoint.
- Parameters: 
  - `uint32_t codepoint`: The Unicode codepoint to evaluate.
- Return: An enumeration value of type `sz_tr29_word_break_t` representing the word break property.
Example:
```c
uint32_t 'a' = 0x61; // ASCII lowercase 'a'
sz_rune_word_break_property('a'); // Returns sz_tr29_word_break_aletter_k (8)
```
2. `sz_utf8_word_find_boundary(const char* str, size_t len)`
- Description: Finds the next word boundary in a UTF-8 encoded string according to TR29 rules.
- Parameters:
  - `const char* str`: Pointer to the start of the UTF-8 string.
  - `size_t len`: Length of the string in bytes.
Example:
```c
char str[] = "hello world";
size_t length = strlen(str);
sz_utf8_word_find_boundary(str, length); // Returns 5 (position after 'o' in "hello")
```
3. `sz_utf8_word_rfind_boundary(const char* str, size_t len, size_t index)`
- Description: Finds the previous word boundary before a specified position in a UTF-8 string.
- Parameters:
  - `const char* str`: Pointer to the start of the UTF-8 string.
  - `size_t len`: Length of the string in bytes.
  - `size_t index`: The position from which to search backward.
Example:
```c
char str[] = "world";
size_t length = strlen(str);
sz_utf8_word_rfind_boundary(str, length, 7); // Returns 6 (position before 'd' in "world")
```
NOTE: These functions enable TR29-compliant word boundary detection for UTF-8 strings, useful for text processing tasks like 
tokenization.
### utf8.h
1. `sz_utf8_count`
- Description: Counts the number of UTF-8 characters in a string by analyzing byte patterns.
- Parameters:
  - `text`: Pointer to the UTF-8 string.
  - `length`: Number of bytes in the string.
- Example:
  ```c
  size_t char_count = sz_utf8_count("Hello, World!", 13);
  printf("Character count: %zu\n", char_count); // Outputs 12
  ```
2. `sz_utf8_find_nth`
- Description: Finds the position of the Nth UTF-8 character in a string.
- Parameters:
  - `text`: Pointer to the string.
  - `length`: String length in bytes.
  - `n`: Zero-based index of the character to find.
- Example:
  ```c
  char const *pos = sz_utf8_find_nth(text, text_length, 1000);
  if (pos != NULL) {
      printf("Found at position: %zu\n", pos - text);
  }
  ```
3. `sz_utf8_find_newline`
- Description: Locates the first newline character in a string, including CRLF.
- Parameters:
  - `text`: String pointer.
  - `length`: String length.
  - `matched_length`: Pointer to store the matched bytes of the newline.
- Example:
  ```c
  char const *newline = sz_utf8_find_newline(text, text_length, &ml);
  if (newline != SZ_NULL_CHAR) {
      printf("Newline found at: %zu\n", newline - text); // Outputs position
  }
  ```
4. `sz_utf8_find_whitespace`
- Description: Finds the first whitespace character as per Unicode.
- Parameters:
  - `text`: String pointer.
  - `length`: Length of the string.
  - `matched_length`: Pointer to store the matched bytes.
- Example:
  ```c
  char const *space = sz_utf8_find_whitespace(text, text_length, &ml);
  if (space != SZ_NULL_CHAR) {
      printf("Whitespace found at: %zu\n", space - text); // Outputs position
  }
  ```
5. `sz_utf8_unpack_chunk`
- Description: Converts a UTF-8 string to UTF-32 codepoints in chunks.
- Parameters:
  - `text`: Pointer to the UTF-8 string.
  - `length`: String length.
  - `runes`: Buffer for output codepoints.
  - `runes_capacity`: Buffer size.
  - `runes_unpacked`: Number of codepoints unpacked.
- Example:
  ```c
  sz_rune_t runes[64];
  sz_size_t unpacked;
  text_ptr = sz_utf8_unpack_chunk(text, length, runes, sizeof(runes)/sizeof(sz_rune_t), &unpacked);
  for (size_t i = 0; i < unpacked; ++i) {
      printf("Codepoint: %ju\n", runes[i]);
  }
  ```
### Summary of Exported Types in StringZilla Library
1. sz_u8_t, sz_u16_t, sz_u32_t, sz_u64_t
   - Description: Unsigned integer types with 8, 16, 32, and 64 bits respectively.
   - Usage: Used for operations requiring unsigned integers of specific bit sizes.
2. sz_i8_t, sz_i16_t, sz_i32_t, sz_i64_t
   - Description: Signed integer types with 8, 16, 32, and 64 bits respectively.
   - Usage: Used for operations requiring signed integers of specific bit sizes.
3. sz_size_t
   - Description: Unsigned integer type identical in size to a pointer.
   - Usage: Commonly used for representing memory addresses or object counts.
4. sz_ssize_t
   - Description: Signed counterpart of sz_size_t, same size as a pointer.
   - Usage: Used for indices and lengths where negative values may be needed.
5. sz_ptr_t
   - Description: Pointer to a C-style string (char*).
   - Usage: Points to mutable string data in memory.
6. sz_cptr_t
   - Description: Pointer to a const C-style string (const char*).
   - Usage: Points to immutable string data for read-only operations.
7. sz_bool_t
   - Description: Boolean type with two values: sz_false_k (0) and sz_true_k (1).
   - Usage: Used in functions needing a binary true/false response.
8. sz_ordering_t
   - Description: Enumeration for comparison results (-1, 0, 1) representing less than, equal, greater than.
   - Usage: Determines the order of elements during comparisons.
9. sz_rune_t
   - Description: 32-bit type for Unicode code points (UTF-32).
   - Usage: Represents a single Unicode character or code point.
10. sz_rune_length_t
    - Description: Enumerates the number of bytes in a UTF8-encoded rune (1 to 4).
    - Usage: Used in functions handling UTF8 encoding and decoding.
11. sz_error_cost_t
    - Description: Signed integer type for substitution costs in string alignment.
    - Usage: Represents the cost associated with character substitutions during alignment operations.
12. sz_string_view_t
    - Description: POD structure containing a pointer to a string's start and its length.
    - Usage: Provides a lightweight view of a substring without owning the data.
13. sz_memory_allocator_t
    - Description: Structure encapsulating memory allocation functions and a handle.
    - Usage: Manages dynamic memory within StringZilla functions, allowing custom allocators.
14. sz_sequence_t
    - Description: Structure representing an ordered collection of strings.
    - Usage: Used for sequences of strings, supporting operations like membership checks and ordering.
15. sz_byteset_t
    - Description: Union type using bitfields to represent a set of 256 possible byte values.
    - Usage: Efficiently stores allowed or banned bytes for filtering operations.
16. sz_similarity_locality_t, sz_similarity_objective_t, sz_similarity_gaps_t
    - Description: Enums defining locality, objective, and gap models for string similarity algorithms.
    - Usage: Configures the behavior of alignment and scoring functions.
17. sz_status_t
    - Description: Enumeration indicating various statuses (success, errors).
    - Usage: Returned by functions to indicate success or specific error conditions.
18. sz_capability_t
    - Description: Enumerates target architecture capabilities for SIMD and other optimizations.
    - Usage: Determines supported features for dynamic dispatch in the library.
19. sz_u16_vec_t, sz_u32_vec_t, sz_u64_vec_t
    - Description: Helper structures for simplifying work with 16-bit, 32-bit, and 64-bit words.
    - Usage: Used in functions needing to handle multi-byte operations efficiently.
20. sz_u128_vec_t, sz_u256_vec_t, sz_u512_vec_t
    - Description: SIMD vector types for x86 and Arm architectures (128, 256, 512 bits).
    - Usage: Facilitates vectorized operations on large data chunks using SIMD instructions.

