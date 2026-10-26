#ifndef LINE_H
#define LINE_H

/* 返回 1 取到一项，0 没有了，-1 这一项超过缓冲区。 */
int take_token(char **p, char *out, int cap);
int take_need(char **p, char *out, int cap, const char *what);
int rest_blank(const char *p);
int reject(const char *msg);

/* 一条命令真要打印时先调用：两条有内容的命令之间空一行。 */
void output_begin(void);

int command_loop(const char *hint, int (*run)(char *line));

#endif
