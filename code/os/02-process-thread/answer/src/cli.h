#ifndef CLI_H
#define CLI_H

#include <stdbool.h>
#include <stddef.h>

/* 读一条非空输入。碰到文件结束返回 false。提示文字只在交互运行时出现。 */
bool cli_read(char *buf, size_t n, const char *prompt_text);

#endif
