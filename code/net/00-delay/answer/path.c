/*
 * 同一组分组：数据报每组自选路径，虚电路先建一条路再全部沿它走。
 * 比较的是到达顺序。链路时延由输入给出，不再拆成发送时延和传播时延。
 */
#include <stdio.h>
#include <string.h>

#include "prompt.h"
#include "qty.h"
#include "route.h"

static int copy_name(char *dst, const char *src) {
    if (src[0] == '\0' || strlen(src) >= ROUTE_NAME) {
        fprintf(stderr, "结点名要在 1 到 %d 个字符之间\n", ROUTE_NAME - 1);
        return 1;
    }
    memcpy(dst, src, strlen(src) + 1);
    return 0;
}

static int take_time(long long *ns, int *flag, const char *what) {
    double value = 0;
    char unit[16];

    if (*flag) {
        fprintf(stderr, "%s 只能写一次\n", what);
        return 1;
    }
    prompt("请输入%s的「数值 单位」：", what);
    if (scanf("%lf %15s", &value, unit) != 2 || !parse_ns(value, unit, ns)) {
        fprintf(stderr, "%s 需要时间，例如 5 ms\n", what);
        return 1;
    }
    *flag = 1;
    return 0;
}

static int take_link(RouteScene *scene) {
    char a[32], b[32], unit[16];
    double value = 0;
    long long ns = 0;
    int i;

    prompt("请输入「端点A 端点B 时延 单位」：");
    if (scanf("%31s %31s %lf %15s", a, b, &value, unit) != 4) {
        fprintf(stderr, "link 需要：端点 端点 时延 单位\n");
        return 1;
    }
    if (strcmp(a, b) == 0 || !parse_ns(value, unit, &ns)) {
        fprintf(stderr, "link 的端点或时延无效\n");
        return 1;
    }
    if (scene->nlinks >= ROUTE_MAX_LINKS) {
        fprintf(stderr, "链路最多 %d 条\n", ROUTE_MAX_LINKS);
        return 1;
    }
    for (i = 0; i < scene->nlinks; i++) {
        int ab = strcmp(scene->links[i].a, a) == 0 && strcmp(scene->links[i].b, b) == 0;
        int ba = strcmp(scene->links[i].a, b) == 0 && strcmp(scene->links[i].b, a) == 0;
        if (ab || ba) {
            fprintf(stderr, "链路 %s-%s 重复\n", a, b);
            return 1;
        }
    }
    if (copy_name(scene->links[scene->nlinks].a, a) != 0 ||
        copy_name(scene->links[scene->nlinks].b, b) != 0) {
        return 1;
    }
    scene->links[scene->nlinks].ns = ns;
    scene->nlinks++;
    return 0;
}

static int take_path(RoutePath *path) {
    int n = 0;
    int i;

    prompt("请输入结点个数和结点名：");
    if (scanf("%d", &n) != 1 || n < 2 || n > ROUTE_MAX_HOPS) {
        fprintf(stderr, "路径结点数要在 2 到 %d 之间\n", ROUTE_MAX_HOPS);
        return 1;
    }
    path->nodes = n;
    for (i = 0; i < n; i++) {
        char name[32];
        if (scanf("%31s", name) != 1 || copy_name(path->name[i], name) != 0) {
            fprintf(stderr, "路径结点名无效\n");
            return 1;
        }
    }
    return 0;
}

static int take_packet(RouteScene *scene) {
    int id = 0;
    int i;

    prompt("请输入分组编号：");
    if (scanf("%d", &id) != 1 || id <= 0) {
        fprintf(stderr, "分组编号要是正整数\n");
        return 1;
    }
    if (scene->npackets >= ROUTE_MAX_PKTS) {
        fprintf(stderr, "分组最多 %d 个\n", ROUTE_MAX_PKTS);
        return 1;
    }
    for (i = 0; i < scene->npackets; i++) {
        if (scene->packets[i].id == id) {
            fprintf(stderr, "分组编号 %d 重复\n", id);
            return 1;
        }
    }
    scene->packets[scene->npackets].id = id;
    if (take_path(&scene->packets[scene->npackets].path) != 0) {
        return 1;
    }
    scene->npackets++;
    return 0;
}

static int dispatch(RouteScene *scene, const char *word) {
    if (strcmp(word, "interval") == 0) {
        return take_time(&scene->interval_ns, &scene->has_interval, "interval");
    }
    if (strcmp(word, "setup") == 0) {
        return take_time(&scene->setup_ns, &scene->has_setup, "setup");
    }
    if (strcmp(word, "link") == 0) {
        return take_link(scene);
    }
    if (strcmp(word, "vc") == 0) {
        if (scene->has_vc) {
            fprintf(stderr, "vc 只能写一次\n");
            return 1;
        }
        if (take_path(&scene->vc) != 0) {
            return 1;
        }
        scene->has_vc = 1;
        return 0;
    }
    if (strcmp(word, "packet") == 0) {
        return take_packet(scene);
    }
    fprintf(stderr, "未知关键字：%s\n", word);
    return 1;
}

int main(void) {
    RouteScene scene;
    char word[32];

    memset(&scene, 0, sizeof scene);
    while (1) {
        prompt("请输入 interval / setup / link / vc / packet，Ctrl+D 结束：");
        if (scanf("%31s", word) != 1) {
            break;
        }
        if (dispatch(&scene, word) != 0) {
            return 1;
        }
    }
    return route_report(stdout, &scene);
}
