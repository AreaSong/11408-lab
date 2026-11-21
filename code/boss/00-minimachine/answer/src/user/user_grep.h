#ifndef USER_GREP_H
#define USER_GREP_H

int user_grep_text(const char *text, const char *pat, char *out, int cap);
int user_grep_line(const char *id, char *out, int cap);

#endif
