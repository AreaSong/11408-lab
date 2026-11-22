/*
 * 2012-42 的入口。每行三个词：share 或 copy，再跟两个单词。
 * share 把最长公共字符后缀接成同一段结点；copy 即使字符相同也不共享。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "prompt.h"
#include "suffix.h"

static SNode *node_new(int data, SNode *next) {
    SNode *s = malloc(sizeof(*s));
    if (s == NULL) {
        return NULL;
    }
    s->data = data;
    s->next = next;
    return s;
}

static int tail_len(const char *a, const char *b) {
    int na = (int)strlen(a);
    int nb = (int)strlen(b);
    int k = 0;
    while (k < na && k < nb && a[na - 1 - k] == b[nb - 1 - k]) {
        k++;
    }
    return k;
}

/* begin 起的后缀。区间为空时返回 NULL，不是失败。 */
static SNode *chain_from(const char *s, int begin) {
    SNode *head = NULL;
    int n = (int)strlen(s);
    for (int i = n - 1; i >= begin; i--) {
        SNode *snode = node_new((unsigned char)s[i], head);
        if (snode == NULL) {
            while (head != NULL) {
                SNode *next = head->next;
                free(head);
                head = next;
            }
            return NULL;
        }
        head = snode;
    }
    return head;
}

static SNode *headed(const char *s, int prefix, SNode *suffix) {
    SNode *head = node_new(0, NULL);
    SNode *tail = head;
    if (head == NULL) {
        return NULL;
    }
    for (int i = 0; i < prefix; i++) {
        tail->next = node_new((unsigned char)s[i], NULL);
        if (tail->next == NULL) {
            return head;
        }
        tail = tail->next;
    }
    tail->next = suffix;
    return head;
}

static void free_until(SNode *p, SNode *stop) {
    while (p != NULL && p != stop) {
        SNode *next = p->next;
        free(p);
        p = next;
    }
}

static void print_start(const SNode *start) {
    if (start == NULL) {
        printf("- suffix=-\n");
        return;
    }
    printf("%c suffix=", start->data);
    for (const SNode *p = start; p != NULL; p = p->next) {
        putchar(p->data);
    }
    putchar('\n');
}

static int show_case(const char *mode, const char *a, const char *b, int share) {
    int na = (int)strlen(a);
    int nb = (int)strlen(b);
    int common = share ? tail_len(a, b) : 0;
    int prefix_a = share ? na - common : 0;
    int prefix_b = share ? nb - common : 0;
    SNode *tail_a = chain_from(a, share ? prefix_a : 0);
    SNode *tail_b = share ? tail_a : chain_from(b, 0);
    SNode *ha = headed(a, prefix_a, tail_a);
    SNode *hb = headed(b, prefix_b, tail_b);
    SNode *start;
    int gap = na > nb ? na - nb : nb - na;
    if (ha == NULL || hb == NULL) {
        fprintf(stderr, "内存不足\n");
        return 0;
    }
    start = list_common_suffix(ha, hb);
    printf("%s str1=%s str2=%s len1=%d len2=%d gap=%d start=", mode, a, b, na, nb, gap);
    print_start(start);
    free_until(ha, start);
    free_until(hb, start);
    free_until(start, NULL);
    return 1;
}

static int word_ok(const char *s) {
    if (s[0] == '\0') {
        return 0;
    }
    for (int i = 0; s[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 'A' || (ch > 'Z' && ch < 'a') || ch > 'z') {
            return 0;
        }
    }
    return 1;
}

int main(void) {
    for (;;) {
        char mode[16];
        char a[64];
        char b[64];
        int got;
        prompt("请输入 share 或 copy，以及两个单词（Ctrl+D 结束）：");
        got = scanf("%15s %63s %63s", mode, a, b);
        if (got == EOF) {
            break;
        }
        if (got != 3 || !word_ok(a) || !word_ok(b) ||
            (strcmp(mode, "share") != 0 && strcmp(mode, "copy") != 0)) {
            fprintf(stderr, "输入格式错误\n");
            return 1;
        }
        if (!show_case(mode, a, b, strcmp(mode, "share") == 0)) {
            return 1;
        }
    }
    return 0;
}
