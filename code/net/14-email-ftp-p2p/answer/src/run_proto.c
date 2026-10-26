#include "run_proto.h"

#include "line.h"
#include "proto.h"

#include <string.h>

static ProtoQuiz quiz;

static int need_end(const char *p) {
    if (!rest_blank(p)) {
        return reject("这一行后面多了内容");
    }
    return 0;
}

static int add_rule(const char *family, const char *id, const char *extra) {
    const char *err = proto_add(&quiz, family, id, extra);

    if (err != NULL) {
        return reject(err);
    }
    return 0;
}

static int cmd_pair(char **p, const char *family) {
    char id[32];
    char extra[16];
    int rc = take_need(p, id, (int)sizeof id, "缺少项目");

    extra[0] = '\0';
    if (rc != 0) {
        return rc;
    }
    if (strcmp(family, "hop") == 0 && strcmp(id, "pull") == 0) {
        rc = take_need(p, extra, (int)sizeof extra, "pull 后面要写 pop3 或 imap");
        if (rc != 0) {
            return rc;
        }
    }
    if (need_end(*p) != 0) {
        return -1;
    }
    return add_rule(family, id, extra[0] == '\0' ? NULL : extra);
}

int run_proto(char *line) {
    char cmd[32];
    char *p = line;
    int rc = take_need(&p, cmd, (int)sizeof cmd, "缺少命令");

    if (rc != 0) {
        return rc;
    }
    if (strcmp(cmd, "reset") == 0) {
        memset(&quiz, 0, sizeof quiz);
        return need_end(p);
    }
    if (strcmp(cmd, "go") == 0) {
        if (need_end(p) != 0) {
            return -1;
        }
        if (quiz.n <= 0) {
            return reject("还没有要判断的内容");
        }
        output_begin();
        proto_show(&quiz, stdout);
        return 0;
    }
    if (strcmp(cmd, "webmail") == 0) {
        if (need_end(p) != 0) {
            return -1;
        }
        return add_rule("webmail", "", NULL);
    }
    if (strcmp(cmd, "stmt") == 0 || strcmp(cmd, "plain") == 0 || strcmp(cmd, "browse") == 0 ||
        strcmp(cmd, "model") == 0 || strcmp(cmd, "transport") == 0 || strcmp(cmd, "hop") == 0) {
        return cmd_pair(&p, cmd);
    }
    return reject("命令用 stmt、plain、browse、model、transport、hop、webmail、go 或 reset");
}
