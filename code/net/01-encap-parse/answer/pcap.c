#include <stdio.h>
#include <string.h>

#include "bytes.h"
#include "pcapio.h"
#include "prompt.h"

static int rest_empty(const char *s, int used) {
    while (s[used] == ' ' || s[used] == '\t') {
        used++;
    }
    return s[used] == '\0';
}

static int next_line(char *buf, int cap, const char *tip) {
    size_t n;

    prompt("%s", tip);
    if (!fgets(buf, cap, stdin)) {
        return 0;
    }
    if (strchr(buf, '\n') == NULL && !feof(stdin)) {
        return -1;
    }
    n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
        buf[--n] = '\0';
    }
    return 1;
}

static int run_pcap(void) {
    char line[64];
    unsigned char buf[PCAP_BYTES_MAX];
    char err[160];
    int n = 0;
    int bad = 0;
    int rc = next_line(line, (int)sizeof line, "请输入 pcap 字节数：");

    if (rc <= 0 || sscanf(line, "%d", &n) != 1 || n < 24 || n > PCAP_BYTES_MAX) {
        fprintf(stderr, "pcap 的字节数要在 24 到 %d 之间\n", PCAP_BYTES_MAX);
        return 1;
    }
    prompt("请输入 %d 个十六进制字节：", n);
    rc = read_hex_bytes(stdin, buf, n, &bad);
    if (rc == 0) {
        fprintf(stderr, "十六进制在第 %d 个字节之前结束了\n", bad);
        return 1;
    }
    if (rc < 0) {
        fprintf(stderr, "第 %d 个字节不是两位十六进制\n", bad);
        return 1;
    }
    if (!pcap_report(stdout, buf, n, err, (int)sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    return 0;
}

static int run_file(void) {
    char path[512];
    unsigned char buf[PCAP_BYTES_MAX];
    char err[160];
    FILE *fp;
    size_t n;
    int extra;
    int rc = next_line(path, (int)sizeof path, "请输入 pcap 文件路径：");

    err[0] = '\0';
    if (rc <= 0 || path[0] == '\0') {
        fprintf(stderr, "file 缺少路径\n");
        return 1;
    }
    fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "打不开 %s\n", path);
        return 1;
    }
    n = fread(buf, 1, sizeof buf, fp);
    extra = fgetc(fp);
    fclose(fp);
    if (extra != EOF) {
        fprintf(stderr, "pcap 文件超过 %d 字节\n", PCAP_BYTES_MAX);
        return 1;
    }
    if (n == 0 || !pcap_report(stdout, buf, (int)n, err, (int)sizeof err)) {
        fprintf(stderr, "%s\n", n == 0 ? "pcap 文件是空的" : err);
        return 1;
    }
    return 0;
}

static int dispatch(const char *cmd) {
    if (strcmp(cmd, "pcap") == 0) {
        return run_pcap();
    }
    if (strcmp(cmd, "file") == 0) {
        return run_file();
    }
    fprintf(stderr, "未知命令：%s\n", cmd);
    return 1;
}

int main(void) {
    char line[64];
    int first = 1;

    while (1) {
        char cmd[32];
        int used = 0;
        int rc = next_line(line, (int)sizeof line, "请输入命令（pcap / file），Ctrl+D 结束：");
        if (rc == 0) {
            break;
        }
        if (rc < 0) {
            fprintf(stderr, "命令这一行太长\n");
            return 1;
        }
        if (line[0] == '\0') {
            continue;
        }
        if (sscanf(line, "%31s %n", cmd, &used) != 1 || !rest_empty(line, used)) {
            fprintf(stderr, "命令只能是 pcap 或 file\n");
            return 1;
        }
        if (!first) {
            fputc('\n', stdout);
        }
        first = 0;
        if (dispatch(cmd) != 0) {
            return 1;
        }
    }
    return 0;
}
