#include "parser/brace_pat.h"
#include "stringzilla/stringzilla.h"
#include <stddef.h>
#include <stdbool.h>

void brace_pat_scan(const char *buf, size_t len, BracePatCb cb, void *user_data) {
    if(!buf || !cb || len < 2) return;

    size_t cur = 0;
    /* candidate bytes: '-', '}' */
    char cand[2]; cand[0] = '-'; cand[1] = '}';

    while(cur + 1 < len) {
        const char *found = sz_find_byte_from(buf + cur, len - cur, cand, 2);
        if(!found) return;
        size_t p = (size_t)(found - buf);
        unsigned char c = (unsigned char)buf[p];

        if(c == '-') {
            if(p + 1 < len && buf[p + 1] == '{') {
                cb(BRACE_PAT_OPEN, p, user_data);
                cur = p + 2;
                continue;
            }
            cur = p + 1;
            continue;
        }

        if(c == '}') {
            if(p + 1 < len && buf[p + 1] == '-') {
                cb(BRACE_PAT_CLOSE, p, user_data);
                cur = p + 2;
                continue;
            }
            cur = p + 1;
            continue;
        }

        cur = p + 1;
    }
}
