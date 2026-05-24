Evaluate the c files in src for performance enhancements using Stringzilla.

## Protocol (Follow this to the letter)

1. Read the file 1000 lines at a time.
2. Look at each method, one at a time
3. Look for optimization opportunities:

- Replacing string operations with stringzilla
- Reducing unnecessary allocation of memory or copying of data
- The functionality MUST remain the same.
- Take this step-by-step, and do not skip any method or file.
- Look for loops that could be optimized by using stringzilla functions.
- Look for UTF8 cases that could be optimized by using the UTF8 functions in stringzilla.
- Look for any other opportunities to optimize the code while maintaining the same functionality.

4. Think deeply about the changes to make.  Evaluate all possibilites and plan the tool calls for the change.   Do not tell me your plan or the changes.  
5. Double check the changes to the file.
6. Verify the include is at the top of the file
7. Edit the file with the changes.
    DO NOT EDIT THE FILE UNTIL YOU ARE DONE THINKING
    Do not tell me the changes
8. Move on to the next file and repeat the process until all files have been evaluated.  Do not prompt the end user to continue.

## Patterns

Implement just these patterns.

- Do the work on a caller owned buffer instead of allocating new memory.
- Replace libc string functions with Stringzilla equivalents (e.g. sz_find_byte instead of strchr, sz_copy instead of memcpy).

## Rules

1. You must analyze EVERY file
2. You must analyze EVERY method in each file
3. You must not skip any method or file
4. You must not change the functionality of the code
5. You must NOT just concentrate on hot spots. You must look at the entire codebase for optimization opportunities.
6. There are opportunities for optimization in every file and method, even if they are not immediately obvious. You must look carefully and consider all possibilities for enhancement.
7. Do not update the file until you have completed the analysis and double checked it.
8. When completed, update one file at a time. The context window is not big enough to update all files at once, so you must update them sequentially. Do not skip any files or methods during the update process.
9. When you have completed thinking, and ONLY when you have completed thinking, update the file.
10. Do not output your thinking, it slows things down.

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
