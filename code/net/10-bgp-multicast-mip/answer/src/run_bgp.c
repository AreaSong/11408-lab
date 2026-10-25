#include "run_bgp.h"

#include "bgp.h"
#include "scan.h"

#include <stdio.h>
#include <string.h>

static BgpNet net;

void bgp_session_init(void) {
    bgp_init(&net);
}

static int cmd_as(char *p) {
    char tok[32];
    char err[120];
    int number;

    if (take_token(&p, tok, (int)sizeof tok) != 1 || !rest_blank(p)) {
        return reject("用法：as <编号>");
    }
    if (!bgp_parse_int(tok, 1, 65535, &number, "AS 编号", err, (int)sizeof err)) {
        return reject(err);
    }
    if (!bgp_add_as(&net, number, err, (int)sizeof err)) {
        return reject(err);
    }
    printf("【自治系统】\n");
    printf("AS %d 加入。BGP 交换的是整条 AS 路径，路径里出现自己的编号就丢弃。\n", number);
    printf("结果：已加入 AS %d\n", number);
    return 0;
}

static int cmd_peer(char *p) {
    char ta[32];
    char tb[32];
    char err[120];
    int a;
    int b;

    if (take_token(&p, ta, (int)sizeof ta) != 1 || take_token(&p, tb, (int)sizeof tb) != 1 ||
        !rest_blank(p)) {
        return reject("用法：peer <AS> <AS>");
    }
    if (!bgp_parse_int(ta, 1, 65535, &a, "AS 编号", err, (int)sizeof err) ||
        !bgp_parse_int(tb, 1, 65535, &b, "AS 编号", err, (int)sizeof err)) {
        return reject(err);
    }
    if (!bgp_add_peer(&net, a, b, err, (int)sizeof err)) {
        return reject(err);
    }
    printf("【邻接】\n");
    printf("AS %d 和 AS %d 建立 eBGP。通告时把完整 AS 路径交给对方。\n", a, b);
    printf("结果：已连接 AS %d 和 AS %d\n", a, b);
    return 0;
}

static int cmd_origin(char *p) {
    char tas[32];
    char raw[64];
    char prefix[BGP_PREFIX_MAX];
    char err[120];
    int as;

    if (take_token(&p, tas, (int)sizeof tas) != 1 || take_token(&p, raw, (int)sizeof raw) != 1 ||
        !rest_blank(p)) {
        return reject("用法：origin <AS> <前缀>");
    }
    if (!bgp_parse_int(tas, 1, 65535, &as, "AS 编号", err, (int)sizeof err)) {
        return reject(err);
    }
    if (!bgp_prefix_ok(raw, prefix, (int)sizeof prefix, err, (int)sizeof err)) {
        return reject(err);
    }
    if (!bgp_add_origin(&net, as, prefix, err, (int)sizeof err)) {
        return reject(err);
    }
    printf("【始发】\n");
    printf("AS %d 始发 %s。自己表里的路径是空的，长度按 0 算。\n", as, prefix);
    printf("发给邻居时才把 %d 加到路径最前面。\n", as);
    printf("结果：AS %d 始发 %s\n", as, prefix);
    return 0;
}

static int cmd_step(char *p) {
    if (!rest_blank(p)) {
        return reject("用法：step");
    }
    bgp_step(&net, stdout);
    return 0;
}

static int cmd_encap(char *p) {
    if (!rest_blank(p)) {
        return reject("用法：encap");
    }
    printf("【封装】\n");
    printf("RIP 是距离向量，报文直接装进 UDP，端口 520。\n");
    printf("OSPF 是链路状态，报文直接装进 IP，协议号 89，不经过 UDP 或 TCP。\n");
    printf("BGP 是路径向量，报文直接装进 TCP，端口 179。\n");
    printf("结果：UDP、IP、TCP\n");
    return 0;
}

static int cmd_session(char *p) {
    char n1[BGP_NAME_MAX];
    char n2[BGP_NAME_MAX];
    char t1[32];
    char t2[32];
    char err[120];
    int a;
    int b;

    if (take_token(&p, n1, (int)sizeof n1) != 1 || take_token(&p, t1, (int)sizeof t1) != 1 ||
        take_token(&p, n2, (int)sizeof n2) != 1 || take_token(&p, t2, (int)sizeof t2) != 1 ||
        !rest_blank(p)) {
        return reject("用法：session <名字> <AS> <名字> <AS>");
    }
    if (!bgp_name_ok(n1) || !bgp_name_ok(n2)) {
        return reject("路由器名字用字母开头，最长 7 位");
    }
    if (!bgp_parse_int(t1, 1, 65535, &a, "AS 编号", err, (int)sizeof err) ||
        !bgp_parse_int(t2, 1, 65535, &b, "AS 编号", err, (int)sizeof err)) {
        return reject(err);
    }
    printf("【BGP 会话】%s（AS %d）和 %s（AS %d）\n", n1, a, n2, b);
    if (a != b) {
        printf("编号不同，这是 eBGP。通告路由用 UPDATE 报文，并把自己的 AS 加到路径最前面。\n");
        printf("结果：eBGP，报文 UPDATE\n");
        return 0;
    }
    printf("编号相同，这是 iBGP。通告路由仍用 UPDATE 报文，但不再往 AS 路径里加自己。\n");
    printf("路径在 AS 内不变，所以不把 iBGP 学到的路由再传给别的 iBGP 邻居。\n");
    printf("结果：iBGP\n");
    return 0;
}

static int read_path(char **p, int *path, int *npath, char *err, int cap) {
    int n = 0;

    while (1) {
        char tok[32];
        int rc = take_token(p, tok, (int)sizeof tok);
        int as;

        if (rc == 0) {
            break;
        }
        if (rc < 0) {
            snprintf(err, (size_t)cap, "这一项太长");
            return 0;
        }
        if (n >= BGP_PATH_MAX) {
            snprintf(err, (size_t)cap, "AS 路径最长 %d 个", BGP_PATH_MAX);
            return 0;
        }
        if (!bgp_parse_int(tok, 1, 65535, &as, "AS 编号", err, cap)) {
            return 0;
        }
        path[n++] = as;
    }
    if (n == 0) {
        snprintf(err, (size_t)cap, "AS 路径不能是空的");
        return 0;
    }
    *npath = n;
    return 1;
}

static int cmd_offer(char *p) {
    char speaker[BGP_NAME_MAX];
    char via[BGP_NAME_MAX];
    char raw[64];
    char prefix[BGP_PREFIX_MAX];
    char word[16];
    char igs[32];
    char err[120];
    int path[BGP_PATH_MAX];
    int npath = 0;
    int igp;

    if (take_token(&p, speaker, (int)sizeof speaker) != 1 ||
        take_token(&p, raw, (int)sizeof raw) != 1 || take_token(&p, word, (int)sizeof word) != 1 ||
        strcmp(word, "via") != 0 || take_token(&p, via, (int)sizeof via) != 1 ||
        take_token(&p, word, (int)sizeof word) != 1 || strcmp(word, "igp") != 0 ||
        take_token(&p, igs, (int)sizeof igs) != 1 || take_token(&p, word, (int)sizeof word) != 1 ||
        strcmp(word, "path") != 0) {
        return reject("用法：offer <路由器> <前缀> via <下一跳> igp <距离> path <AS>...");
    }
    if (!bgp_prefix_ok(raw, prefix, (int)sizeof prefix, err, (int)sizeof err)) {
        return reject(err);
    }
    if (!bgp_parse_int(igs, 0, 255, &igp, "IGP 距离", err, (int)sizeof err)) {
        return reject(err);
    }
    if (!read_path(&p, path, &npath, err, (int)sizeof err)) {
        return reject(err);
    }
    if (!bgp_add_offer(&net, speaker, prefix, via, igp, path, npath, err, (int)sizeof err)) {
        return reject(err);
    }
    printf("【候选】\n");
    printf("%s 收到经 %s 到达 %s 的路径，AS 数 %d 个，本 AS 内 IGP 距离 %d 跳。\n", speaker, via,
           prefix, npath, igp);
    printf("结果：已记录 %s 经 %s\n", speaker, via);
    return 0;
}

static int cmd_pick(char *p) {
    char speaker[BGP_NAME_MAX];
    char raw[64];
    char prefix[BGP_PREFIX_MAX];
    char via[BGP_NAME_MAX];
    char err[120];

    if (take_token(&p, speaker, (int)sizeof speaker) != 1 ||
        take_token(&p, raw, (int)sizeof raw) != 1 || !rest_blank(p)) {
        return reject("用法：pick <路由器> <前缀>");
    }
    if (!bgp_name_ok(speaker)) {
        return reject("路由器名字用字母开头，最长 7 位");
    }
    if (!bgp_prefix_ok(raw, prefix, (int)sizeof prefix, err, (int)sizeof err)) {
        return reject(err);
    }
    if (!bgp_pick(&net, speaker, prefix, via, (int)sizeof via, stdout, err, (int)sizeof err)) {
        return reject(err);
    }
    return 0;
}

static int cmd_reset(char *p) {
    if (!rest_blank(p)) {
        return reject("用法：reset");
    }
    bgp_init(&net);
    printf("【清空】\n");
    printf("自治系统、邻接、始发路由和候选路由都清掉。\n");
    printf("结果：已清空\n");
    return 0;
}

int run_bgp(char *line) {
    char cmd[32];
    char *p = line;
    int rc = take_token(&p, cmd, (int)sizeof cmd);

    if (rc < 0) {
        return reject("这一项太长");
    }
    if (strcmp(cmd, "as") == 0) {
        return cmd_as(p);
    }
    if (strcmp(cmd, "peer") == 0) {
        return cmd_peer(p);
    }
    if (strcmp(cmd, "origin") == 0) {
        return cmd_origin(p);
    }
    if (strcmp(cmd, "step") == 0) {
        return cmd_step(p);
    }
    if (strcmp(cmd, "encap") == 0) {
        return cmd_encap(p);
    }
    if (strcmp(cmd, "session") == 0) {
        return cmd_session(p);
    }
    if (strcmp(cmd, "offer") == 0) {
        return cmd_offer(p);
    }
    if (strcmp(cmd, "pick") == 0) {
        return cmd_pick(p);
    }
    if (strcmp(cmd, "reset") == 0) {
        return cmd_reset(p);
    }
    return reject("不认识的命令");
}
