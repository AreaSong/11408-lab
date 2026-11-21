#include "user/user_sh.h"

#include <string.h>

static int on_sep(char ch, UserCmd *cmd, int *at, int cap, int *want, int *nout) {
    if (ch == '>') {
        *want = 1;
        return 1;
    }
    if (ch == ' ' || ch == '\t') {
        return 1;
    }
    if (*want || cmd[*at].argc <= 0) {
        return -1;
    }
    if (ch == '|') {
        if (cmd[*at].redir[0] != '\0' || *at + 1 >= cap) {
            return -1;
        }
        (*at)++;
        return 1;
    }
    *nout = *at + 1;
    return 0;
}

static int take_tok(UserCmd *cmd, const char *tok, int want_redir) {
    if (want_redir) {
        if (cmd->redir[0] != '\0') {
            return -1;
        }
        memcpy(cmd->redir, tok, strlen(tok) + 1);
        return 0;
    }
    if (cmd->argc >= USER_NARG) {
        return -1;
    }
    memcpy(cmd->arg[cmd->argc], tok, strlen(tok) + 1);
    cmd->argc++;
    return 0;
}

int user_sh_parse(const char *line, UserCmd *cmd, int cap, int *nout) {
    int i = 0;
    int at = 0;
    int want = 0;
    int tn = 0;
    char tok[USER_WORD];
    if (line == NULL || cmd == NULL || nout == NULL || cap < 1) {
        return -1;
    }
    memset(cmd, 0, sizeof(UserCmd) * (size_t)cap);
    for (;;) {
        char ch = line[i];
        int cut = ch == '\0' || ch == ' ' || ch == '\t' || ch == '|' || ch == '>';
        if (!cut) {
            if (tn + 1 >= USER_WORD) {
                return -1;
            }
            tok[tn++] = ch;
            i++;
            continue;
        }
        if (tn > 0) {
            tok[tn] = '\0';
            if (take_tok(&cmd[at], tok, want) != 0) {
                return -1;
            }
            want = 0;
            tn = 0;
        }
        {
            int step = on_sep(ch, cmd, &at, cap, &want, nout);
            if (step == 0) {
                return 0;
            }
            if (step < 0) {
                return -1;
            }
        }
        i++;
    }
}

int user_sh_jobs(const UserCmd *cmd, int n, UserJob *job) {
    int i;
    if (cmd == NULL || job == NULL || n < 1) {
        return -1;
    }
    memset(job, 0, sizeof *job);
    job->ncmd = n;
    job->forks = n;
    job->execs = n;
    job->waits = n;
    for (i = 0; i < n; i++) {
        if (i + 1 < n) {
            job->pipes++;
        }
        if (cmd[i].redir[0] != '\0') {
            job->redir++;
        }
    }
    return 0;
}
