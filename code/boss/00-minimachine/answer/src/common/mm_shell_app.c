#include "common/mm_shell_stage.h"

#include "kernel/k_sock.h"
#include "net/net_cong.h"
#include "net/net_dns.h"
#include "net/net_flow.h"
#include "net/net_http.h"
#include "net/net_mail.h"
#include "net/net_sock.h"
#include "net/net_tcp.h"
#include "net/net_udp.h"

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
    char buf[192];
    const char *id = rest(line, word);
    if (id[0] == '\0' || fn(id, buf, (int)sizeof buf) != 0) {
        fprintf(out, "错误：%s 参数不对\n", word);
        return -1;
    }
    return emit(out, word, id, buf);
}

static int cmd_app(char *line, FILE *out, MmMachine **machine, MmShellOpt *opt) {
    const char *arg;
    if (strcmp(line, "session") == 0) {
        char body[1200];
        if (!opt->app_on) {
            fprintf(out, "错误：先执行 app on\n");
            return -1;
        }
        if (net_session_line(body, (int)sizeof body) != 0) {
            return -1;
        }
        fputs(body, out);
        return 1;
    }
    if (strcmp(line, "rto") == 0) {
        char buf[192];
        if (net_rto_line(buf, (int)sizeof buf) != 0) {
            return -1;
        }
        return emit(out, "rto", "", buf);
    }
    if (!starts(line, "app")) {
        return 0;
    }
    arg = rest(line, "app");
    if (strcmp(arg, "on") != 0 && strcmp(arg, "off") != 0) {
        fprintf(out, "错误：app on 或 off\n");
        return -1;
    }
    opt->app_on = strcmp(arg, "on") == 0;
    if (*machine != NULL) {
        if (opt->app_on) {
            k_app_on(*machine);
        } else {
            k_app_off(*machine);
        }
    }
    fprintf(out, "app %s\n", arg);
    return 1;
}

int mm_shell_app(char *line, FILE *out, MmMachine **machine, MmShellOpt *opt) {
    if (starts(line, "udp")) {
        return one(out, line, "udp", net_udp_line);
    }
    if (starts(line, "conn")) {
        return one(out, line, "conn", net_tcp_line);
    }
    if (starts(line, "cwnd")) {
        return one(out, line, "cwnd", net_cong_line);
    }
    if (starts(line, "flow")) {
        return one(out, line, "flow", net_flow_line);
    }
    if (starts(line, "dns")) {
        return one(out, line, "dns", net_dns_line);
    }
    if (starts(line, "page")) {
        return one(out, line, "page", net_http_line);
    }
    if (starts(line, "mail")) {
        return one(out, line, "mail", net_mail_line);
    }
    if (starts(line, "ftp")) {
        return one(out, line, "ftp", net_ftp_line);
    }
    if (starts(line, "p2p")) {
        return one(out, line, "p2p", net_p2p_line);
    }
    if (starts(line, "aexam")) {
        return one(out, line, "aexam", net_aexam_line);
    }
    return cmd_app(line, out, machine, opt);
}
