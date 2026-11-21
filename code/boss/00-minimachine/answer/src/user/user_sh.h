#ifndef USER_SH_H
#define USER_SH_H

/* 一条命令最多 4 段管道。重定向只允许出现在最后一段，因为前面的输出要流向下一条。 */
#define USER_NCMD 4
#define USER_NARG 16
#define USER_WORD 48

typedef struct {
    int argc;
    char arg[USER_NARG][USER_WORD];
    char redir[USER_WORD];
} UserCmd;

typedef struct {
    int ncmd;
    int pipes;
    int redir;
    int forks;
    int execs;
    int waits;
} UserJob;

int user_sh_parse(const char *line, UserCmd *cmd, int cap, int *n);
int user_sh_jobs(const UserCmd *cmd, int n, UserJob *job);
int user_rt_line(char *out, int cap);

#endif
