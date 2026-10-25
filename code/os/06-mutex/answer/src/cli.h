#ifndef CLI_H
#define CLI_H

#include <stdbool.h>
#include <stddef.h>

/* 读一条非空输入。碰到文件结束返回 false。提示只在交互运行时出现。 */
bool cli_read(char *buf, size_t n, const char *prompt_text);

/* 就地按空白切开。返回词的个数；超过 max 返回 -1。 */
int cli_split(char *line, char **tok, int max);

#endif
