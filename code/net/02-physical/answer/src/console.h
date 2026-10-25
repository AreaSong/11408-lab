#ifndef CONSOLE_H
#define CONSOLE_H

/* 解析主程序的一行命令。成功返回 0，向 stderr 说明原因时返回 1。 */
int run_command(const char *line);

#endif
