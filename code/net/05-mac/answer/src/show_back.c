#include "show_back.h"

#include "show_io.h"

static int cmd_backoff(const char *p) {
    int n = 0;
    long long slot = 0;
    Backoff b;
    char slot_s[32];
    char max_s[32];

    if (!need_key(&p, "n") || !take_int(&p, &n, 1, 16) || !need_key(&p, "slot") ||
        !take_num(&p, parse_ns, &slot) || slot <= 0 || !scan_blank(p)) {
        return err("命令格式不对");
    }
    if (!backoff_calc(n, slot, &b)) {
        return bad_span();
    }
    fmt_us(slot_s, sizeof slot_s, slot);
    printf("【二进制指数退避】\n");
    printf("连续冲突 %d 次。k = min(冲突次数, 10)。\n", n);
    if (b.rmax < 0) {
        printf("第 16 次冲突后不再重传，丢弃这一帧，交给上层。\n");
        printf("结果：放弃\n");
        return 0;
    }
    fmt_us(max_s, sizeof max_s, b.max_ns);
    printf("k = min(%d, 10) = %d。\n", n, b.k);
    printf("r 从 0～%d 里取，一共 %d 个可能。\n", b.rmax, b.rmax + 1);
    printf("争用时间片 = 基本退避时间 = %s。\n", slot_s);
    printf("最短等待是 r = 0，也就是 0 us。\n");
    printf("最长等待是 r = %d，%d × %s = %s。\n", b.rmax, b.rmax, slot_s, max_s);
    printf("还没到第 16 次冲突，这一帧要重传。\n");
    printf("结果：%s\n", max_s);
    return 0;
}

static int read_picks(const char **ps, int *r, int *n) {
    char word[32];
    int rc;

    *n = 0;
    while ((rc = scan_word(ps, word, (int)sizeof word)) == 1) {
        char *end = NULL;
        long value = strtol(word, &end, 10);

        if (end == word || *end != '\0' || value < 0 || value > 1023 || *n >= 15) {
            return 0;
        }
        r[(*n)++] = (int)value;
    }
    return rc == 0 && *n > 0;
}

static int check_picks(const int *r, int n, long long slot, long long *wait) {
    int i;

    for (i = 0; i < n; i++) {
        Backoff b;

        if (!backoff_chosen(i + 1, r[i], slot, &b, &wait[i])) {
            fprintf(stderr, "第 %d 次冲突的 r 必须在 0～%d\n", i + 1, b.rmax);
            return 0;
        }
    }
    return 1;
}

static void print_picks(const int *r, int n, long long slot, const long long *wait) {
    char slot_s[32];
    int i;

    fmt_us(slot_s, sizeof slot_s, slot);
    printf("【退避过程】\n");
    printf("争用时间片 = %s。第 16 次冲突才放弃，这里只走到第 %d 次。\n", slot_s, n);
    for (i = 0; i < n; i++) {
        Backoff b;
        char wait_s[32];

        backoff_calc(i + 1, slot, &b);
        fmt_us(wait_s, sizeof wait_s, wait[i]);
        printf("第 %d 次冲突：k = min(%d, 10) = %d，范围 0～%d，选中 r = %d，等待 %s\n", i + 1, i + 1, b.k,
               b.rmax, r[i], wait_s);
    }
    printf("结果：");
    for (i = 0; i < n; i++) {
        char wait_s[32];

        if (i > 0) {
            printf("，");
        }
        fmt_us(wait_s, sizeof wait_s, wait[i]);
        printf("%s", wait_s);
    }
    printf("\n");
}

static int cmd_picks(const char *p) {
    long long slot = 0;
    int r[15];
    int n = 0;
    long long wait[15];

    if (!need_key(&p, "slot") || !take_num(&p, parse_ns, &slot) || slot <= 0 || !need_key(&p, "r") ||
        !read_picks(&p, r, &n)) {
        return err("命令格式不对");
    }
    if (!check_picks(r, n, slot, wait)) {
        return 1;
    }
    print_picks(r, n, slot, wait);
    return 0;
}

int back_command(const char *cmd, const char *args) {
    if (strcmp(cmd, "backoff") == 0) {
        return cmd_backoff(args);
    }
    return cmd_picks(args);
}
