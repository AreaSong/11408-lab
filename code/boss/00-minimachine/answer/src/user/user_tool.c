#include "user/user_tool.h"

#include "user/user_file.h"
#include "user/user_grep.h"
#include "user/user_net.h"
#include "user/user_sh.h"
#include "user/user_sort.h"
#include "user/user_zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int join_echo(const UserCmd *cmd, char *out, int cap) {
    int i;
    int at = 0;
    out[0] = '\0';
    for (i = 1; i < cmd->argc; i++) {
        int w = snprintf(out + at, (size_t)(cap - at), "%s%s", i > 1 ? " " : "", cmd->arg[i]);
        if (w < 0 || at + w >= cap) {
            return -1;
        }
        at += w;
    }
    return 0;
}

static int is_num(const char *s) {
    int i = 0;
    if (s[0] == '-') {
        i++;
    }
    if (s[i] == '\0') {
        return 0;
    }
    for (; s[i] != '\0'; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return 0;
        }
    }
    return 1;
}

static int say_ok(char *out, int cap) {
    return snprintf(out, (size_t)cap, "ok") > 0 ? 0 : -1;
}

static int run_fs(FsNs *ns, const UserCmd *cmd, const char *in, char *out, int cap) {
    const char *name = cmd->arg[0];
    if (strcmp(name, "mkdir") == 0 && cmd->argc == 2) {
        return user_mkdir(ns, cmd->arg[1]) == 0 ? say_ok(out, cap) : -1;
    }
    if (strcmp(name, "rm") == 0 && cmd->argc == 2) {
        return user_rm(ns, cmd->arg[1]) == 0 ? say_ok(out, cap) : -1;
    }
    if (strcmp(name, "ls") == 0) {
        return user_ls(ns, cmd->argc == 2 ? cmd->arg[1] : "/", out, cap);
    }
    if (strcmp(name, "cat") == 0 && cmd->argc == 1) {
        return snprintf(out, (size_t)cap, "%s", in) < cap ? 0 : -1;
    }
    if (strcmp(name, "cat") == 0 && cmd->argc == 2) {
        return user_cat(ns, cmd->arg[1], out, cap);
    }
    if (strcmp(name, "df") == 0) {
        return user_df(ns, out, cap);
    }
    if (strcmp(name, "ln") == 0 && cmd->argc == 4 && strcmp(cmd->arg[1], "-s") == 0) {
        return user_ln(ns, 1, cmd->arg[2], cmd->arg[3]) == 0 ? say_ok(out, cap) : -1;
    }
    if (strcmp(name, "ln") == 0 && cmd->argc == 3) {
        return user_ln(ns, 0, cmd->arg[1], cmd->arg[2]) == 0 ? say_ok(out, cap) : -1;
    }
    return 1;
}

static int run_one(FsNs *ns, const UserCmd *cmd, const char *in, char *out, int cap) {
    const char *name = cmd->arg[0];
    int fs = run_fs(ns, cmd, in, out, cap);
    if (fs <= 0) {
        return fs;
    }
    if (strcmp(name, "echo") == 0) {
        return join_echo(cmd, out, cap);
    }
    if (strcmp(name, "grep") == 0 && cmd->argc >= 2) {
        char file[160];
        const char *text = in;
        if (cmd->argc == 3) {
            if (user_cat(ns, cmd->arg[2], file, (int)sizeof file) != 0) {
                return -1;
            }
            text = file;
        }
        return user_grep_text(text, cmd->arg[1], out, cap);
    }
    if (strcmp(name, "sort") == 0 && cmd->argc == 2 && is_num(cmd->arg[1])) {
        return user_sort_text(ns, atoi(cmd->arg[1]), in, out, cap);
    }
    if (strcmp(name, "zip") == 0 && cmd->argc == 1) {
        return user_zip_text(in, out, cap);
    }
    if (strcmp(name, "ping") == 0) {
        return user_ping_line(out, cap);
    }
    if (strcmp(name, "curl") == 0) {
        return user_curl_line(out, cap);
    }
    if (strcmp(name, "mail") == 0) {
        return user_mail_tool(out, cap);
    }
    return -1;
}

int user_tool_run(FsNs *ns, const char *line, char *out, int cap) {
    UserCmd cmd[USER_NCMD];
    char cur[192];
    char next[192];
    int n = 0;
    int i;
    if (user_sh_parse(line, cmd, USER_NCMD, &n) != 0) {
        return -1;
    }
    cur[0] = '\0';
    for (i = 0; i < n; i++) {
        if (run_one(ns, &cmd[i], cur, next, (int)sizeof next) != 0) {
            return -1;
        }
        if (i + 1 < n) {
            snprintf(cur, sizeof cur, "%s", next);
        }
    }
    if (cmd[n - 1].redir[0] != '\0') {
        if (user_write(ns, cmd[n - 1].redir, next) != 0) {
            return -1;
        }
        out[0] = '\0';
        return 0;
    }
    if (snprintf(out, (size_t)cap, "%s", next) >= cap) {
        return -1;
    }
    return 0;
}
