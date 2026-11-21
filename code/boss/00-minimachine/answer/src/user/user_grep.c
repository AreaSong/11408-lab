#include "user/user_grep.h"

#include "match.h"

#include <stdio.h>
#include <string.h>

static int kmp_at(const char *text, const char *pat, MatchInfo *info) {
    if (match_kmp(text, pat, 0, 0, NULL, info) == 0) {
        return -1;
    }
    return 0;
}

int user_grep_text(const char *text, const char *pat, char *out, int cap) {
    MatchInfo info;
    char body[160];
    int n;
    if (text == NULL || pat == NULL || pat[0] == '\0' || out == NULL) {
        return -1;
    }
    n = snprintf(body, sizeof body, "%s", text);
    if (n < 0 || n >= (int)sizeof body) {
        return -1;
    }
    while (n > 0 && (body[n - 1] == '\n' || body[n - 1] == ' ')) {
        body[--n] = '\0';
    }
    if (kmp_at(body, pat, &info) != 0) {
        return -1;
    }
    if (info.pos < 0) {
        n = snprintf(out, (size_t)cap, "host=grep kmp pos=%d compares=%d", info.pos, info.compares);
    } else {
        n = snprintf(out, (size_t)cap, "%s\nhost=grep kmp pos=%d compares=%d", body, info.pos, info.compares);
    }
    return n > 0 && n < cap ? 0 : -1;
}

int user_grep_line(const char *id, char *out, int cap) {
    MatchInfo info;
    int n;
    if (id == NULL || out == NULL) {
        return -1;
    }
    if (strcmp(id, "2015-8") == 0) {
        if (kmp_at("abaabaabacacaabaabcc", "abaabc", &info) != 0) {
            return -1;
        }
        n = snprintf(out, (size_t)cap, "i=%d j=%d letter=C", info.next_i, info.next_j);
    } else if (strcmp(id, "2019-9") == 0) {
        if (kmp_at("abaabaabcabaabc", "abaabc", &info) != 0) {
            return -1;
        }
        n = snprintf(out, (size_t)cap, "compares=%d pos=%d letter=B", info.compares, info.pos);
    } else {
        return -1;
    }
    return n > 0 && n < cap ? 0 : -1;
}
