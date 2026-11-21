#include "net/net_slide.h"

#include "cycq.h"

#include <stdio.h>
#include <string.h>

static int span_dist(int seq, int base, int span) { return (seq - base + span) % span; }

static void dump_q(const CycQ *q, int *list, int cap, int *n) {
    int i = q->front;
    *n = 0;
    while (i != q->rear && *n < cap) {
        list[(*n)++] = q->data[i];
        i = (i + 1) % q->cap;
    }
}

static int gbn_ack(CycQ *q, int *base, int next, int ack, int span) {
    int size = span_dist(next, *base, span);
    int dist = span_dist(ack, *base, span);
    int neu;
    int h = 0;
    if (dist >= size) {
        return 0;
    }
    neu = (ack + 1) % span;
    while (!cyc_empty(q)) {
        if (!cyc_head(q, &h) || h == neu) {
            break;
        }
        cyc_deq(q, &h);
    }
    *base = neu;
    return 1;
}

static int fill_window(CycQ *q, int *next, int count) {
    int i;
    for (i = 0; i < count; i++) {
        if (!cyc_enq(q, *next)) {
            return -1;
        }
        (*next)++;
    }
    return 0;
}

static int gbn_case(int nbits, int window, int send_n, const int *acks, int nack, int *list, int cap) {
    CycQ q;
    int base = 0;
    int next = 0;
    int span = 1 << nbits;
    int i;
    int n = 0;
    if (!cyc_init(&q, window + 1) || fill_window(&q, &next, send_n) != 0) {
        return -1;
    }
    for (i = 0; i < nack; i++) {
        gbn_ack(&q, &base, next, acks[i], span);
    }
    dump_q(&q, list, cap, &n);
    return n;
}

static int sr_mark(CycQ *q, int *base, int span, int ack, int *got) {
    int h = 0;
    if (ack < 0 || ack >= span) {
        return -1;
    }
    got[ack] = 1;
    while (!cyc_empty(q)) {
        if (!cyc_head(q, &h) || !got[h]) {
            break;
        }
        cyc_deq(q, &h);
        *base = (*base + 1) % span;
    }
    return 0;
}

static int sr_2011(int *list, int cap) {
    CycQ q;
    int got[8] = {0};
    int base = 0;
    int next = 0;
    int queued[8];
    int nq = 0;
    int n = 0;
    int i;
    int hot[2] = {0, 2};
    if (!cyc_init(&q, 5) || fill_window(&q, &next, 4) != 0 || sr_mark(&q, &base, 8, 1, got) != 0) {
        return -1;
    }
    dump_q(&q, queued, 8, &nq);
    for (i = 0; i < nq && n < cap; i++) {
        if (!got[queued[i]] && (queued[i] == hot[0] || queued[i] == hot[1])) {
            list[n++] = queued[i];
        }
    }
    return n;
}

int net_sr_2024(int *front, int *rear, int *t1, int *t2) {
    CycQ q;
    int got[8] = {0};
    int base = 0;
    int next = 0;
    if (front == NULL || rear == NULL || t1 == NULL || t2 == NULL) {
        return -1;
    }
    if (!cyc_init(&q, 5) || fill_window(&q, &next, 4) != 0) {
        return -1;
    }
    /* F1 丢失。ACK0 让左边界滑到 1，空出来的位置才能发 F4。 */
    if (sr_mark(&q, &base, 8, 0, got) != 0 || !cyc_enq(&q, 4)) {
        return -1;
    }
    *t1 = 4;
    *front = q.front;
    *rear = q.rear;
    sr_mark(&q, &base, 8, 2, got);
    *t2 = 1;
    return cyc_count(&q);
}

static int put_note(char *note, int ncap, const char *text) {
    if (note == NULL || ncap < (int)strlen(text) + 1) {
        return -1;
    }
    snprintf(note, (size_t)ncap, "%s", text);
    return 0;
}

int net_rexmit(const char *id, int *list, int cap, char *note, int ncap) {
    int acks_2009[3] = {0, 2, 3};
    int acks_gbn[2] = {0, 1};
    int n;
    if (id == NULL || list == NULL || cap < 4 || note == NULL) {
        return -1;
    }
    note[0] = '\0';
    if (strcmp(id, "2009-35") == 0) {
        n = gbn_case(4, 8, 8, acks_2009, 3, list, cap);
        return put_note(note, ncap, "GBN") == 0 ? n : -1;
    }
    if (strcmp(id, "2011-35") == 0) {
        n = sr_2011(list, cap);
        return put_note(note, ncap, "SR") == 0 ? n : -1;
    }
    if (strcmp(id, "practice-gbn") == 0) {
        n = gbn_case(3, 5, 5, acks_gbn, 2, list, cap);
        return put_note(note, ncap, "GBN") == 0 ? n : -1;
    }
    if (strcmp(id, "2024-37") == 0) {
        int front = 0;
        int rear = 0;
        int t1 = 0;
        int t2 = 0;
        if (net_sr_2024(&front, &rear, &t1, &t2) < 0 || front != 1 || rear != 0) {
            return -1;
        }
        list[0] = t2;
        if (ncap < 16) {
            return -1;
        }
        note[0] = 't';
        note[1] = '1';
        note[2] = '=';
        note[3] = 'F';
        note[4] = (char)('0' + t1);
        note[5] = ' ';
        note[6] = 't';
        note[7] = '2';
        note[8] = '=';
        note[9] = 'F';
        note[10] = (char)('0' + t2);
        note[11] = '\0';
        return 1;
    }
    return -1;
}
