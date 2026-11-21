#include "common/mm_shell_stage.h"

#include "kernel/k_file.h"
#include "kernel/k_proc.h"
#include "kernel/k_trap.h"
#include "user/user_boot.h"
#include "user/user_grep.h"
#include "user/user_net.h"
#include "user/user_scene.h"
#include "user/user_sh.h"
#include "user/user_sort.h"
#include "user/user_tool.h"
#include "user/user_udp.h"
#include "user/user_zip.h"

#include <stdio.h>
#include <string.h>

static int starts(const char *s, const char *word) {
    size_t n = strlen(word);
    return strncmp(s, word, n) == 0 && (s[n] == '\0' || s[n] == ' ' || s[n] == '\t');
}

static const char *rest(const char *s, const char *word) {
    const char *p = s + strlen(word);
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return p;
}

static int emit(FILE *out, const char *word, const char *id, const char *body) {
    if (id[0] == '\0') {
        fprintf(out, "%s %s\n", word, body);
    } else {
        fprintf(out, "%s %s %s\n", word, id, body);
    }
    return 1;
}

static int one(FILE *out, const char *line, const char *word, int (*fn)(const char *, char *, int)) {
    char buf[640];
    const char *id = rest(line, word);
    if (id[0] == '\0' || fn(id, buf, (int)sizeof buf) != 0) {
        fprintf(out, "错误：%s 参数不对\n", word);
        return -1;
    }
    return emit(out, word, id, buf);
}

static int ensure(MmMachine **machine, uint32_t bytes, MmShellOpt *opt, FILE *out) {
    if (*machine == NULL) {
        *machine = mm_create(bytes);
    }
    if (*machine == NULL || k_boot(*machine) != 0 || k_proc_on(*machine) != 0 || k_fs_on(*machine) != 0) {
        fprintf(out, "错误：shell 没有装上\n");
        return -1;
    }
    opt->shell_on = 1;
    opt->kernel = 1;
    opt->proc = 1;
    opt->fs = 1;
    return 0;
}

static int cmd_shell(const char *arg, FILE *out, MmMachine **machine, uint32_t bytes, MmShellOpt *opt) {
    if (strcmp(arg, "on") == 0) {
        if (ensure(machine, bytes, opt, out) != 0) {
            return -1;
        }
        fprintf(out, "shell on\n");
        return 1;
    }
    if (strcmp(arg, "off") == 0) {
        opt->shell_on = 0;
        fprintf(out, "shell off\n");
        return 1;
    }
    fprintf(out, "错误：shell on 或 off\n");
    return -1;
}

static int cmd_sh(const char *arg, FILE *out, MmMachine **machine, uint32_t bytes, MmShellOpt *opt) {
    char buf[640];
    (void)bytes;
    if (!opt->shell_on || *machine == NULL) {
        fprintf(out, "错误：先执行 shell on\n");
        return -1;
    }
    if (starts(arg, "jobs")) {
        UserCmd cmd[USER_NCMD];
        UserJob job;
        int n = 0;
        if (user_sh_parse(rest(arg, "jobs"), cmd, USER_NCMD, &n) != 0 || user_sh_jobs(cmd, n, &job) != 0) {
            fprintf(out, "错误：jobs 参数不对\n");
            return -1;
        }
        fprintf(out, "sh jobs cmd=%d pipe=%d redir=%d fork=%d exec=%d wait=%d\n", job.ncmd, job.pipes, job.redir,
                job.forks, job.execs, job.waits);
        return 1;
    }
    if (user_tool_run(k_fs_ns(*machine), arg, buf, (int)sizeof buf) != 0) {
        fprintf(out, "错误：sh 没有跑通\n");
        return -1;
    }
    if (buf[0] != '\0') {
        fprintf(out, "%s\n", buf);
    }
    return 1;
}

static int plain(FILE *out, const char *word, int (*fn)(char *, int)) {
    char buf[320];
    if (fn(buf, (int)sizeof buf) != 0) {
        fprintf(out, "错误：%s 失败\n", word);
        return -1;
    }
    return emit(out, word, "", buf);
}

int mm_shell_show(char *line, FILE *out, MmMachine **machine, uint32_t mem_bytes, MmShellOpt *opt) {
    if (starts(line, "shell")) {
        return cmd_shell(rest(line, "shell"), out, machine, mem_bytes, opt);
    }
    if (starts(line, "sh")) {
        return cmd_sh(rest(line, "sh"), out, machine, mem_bytes, opt);
    }
    if (starts(line, "grep")) {
        return one(out, line, "grep", user_grep_line);
    }
    if (starts(line, "sort")) {
        return one(out, line, "sort", user_sort_line);
    }
    if (starts(line, "zip")) {
        return one(out, line, "zip", user_zip_line);
    }
    if (starts(line, "scene")) {
        return one(out, line, "scene", user_scene_line);
    }
    if (strcmp(line, "boot") == 0) {
        return plain(out, "boot", user_boot_line);
    }
    if (strcmp(line, "visit") == 0) {
        return plain(out, "visit", user_visit_line);
    }
    if (strcmp(line, "ping") == 0) {
        return plain(out, "ping", user_ping_line);
    }
    if (strcmp(line, "curl") == 0) {
        return plain(out, "curl", user_curl_line);
    }
    if (strcmp(line, "rt") == 0) {
        return plain(out, "rt", user_rt_line);
    }
    if (strcmp(line, "udpdemo") == 0) {
        return plain(out, "udpdemo", user_udp_once);
    }
    return 0;
}
