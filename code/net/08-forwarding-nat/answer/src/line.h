#ifndef LINE_H
#define LINE_H

/* 返回 1 取到一项，0 没有了，-1 这一项超过缓冲区。 */
int take_token(char **p, char *out, int cap);
int rest_blank(const char *p);
int reject(const char *msg);
int command_loop(const char *hint, int (*run)(char *line));

#endif
