#include <string.h>

#include "cycq.h"
#include "deque.h"
#include "ends.h"
#include "linkq.h"
#include "sizeq.h"
#include "tagq.h"
#include "tailq.h"
#include "test.h"

static void test_formula(void) {
    CHECK_EQ(cyc_length(10, 4, 1), 7);
    CHECK_EQ(cyc_length(8, 6, 2), 4);
    CHECK_EQ(cyc_length(8, 0, 7), 7);
    CHECK_EQ(cyc_length(8, 5, 5), 0);
    CHECK(!cyc_init(NULL, 8));
    {
        CycQ queue;
        CHECK(!cyc_init(&queue, 1));
        CHECK(!cyc_init(&queue, CYC_MAX + 1));
    }
}

static void test_cyc_oracle(void) {
    int buf[256];
    int lo = 0;
    int hi = 0;
    CycQ queue;
    int kept = 99;
    CHECK(cyc_init(&queue, 8));
    CHECK(cyc_empty(&queue));
    CHECK(!cyc_deq(&queue, &kept));
    CHECK_EQ(kept, 99);
    for (int i = 0; i < 200; i++) {
        int len = hi - lo;
        int at;
        if (len < 7 && (len == 0 || i % 3 != 0)) {
            CHECK(cyc_enq(&queue, i + 1));
            buf[hi++] = i + 1;
        } else {
            int value = -1;
            CHECK(cyc_deq(&queue, &value));
            CHECK_EQ(value, buf[lo]);
            lo++;
        }
        CHECK_EQ(cyc_count(&queue), hi - lo);
        CHECK_EQ(cyc_count(&queue), cyc_length(queue.cap, queue.front, queue.rear));
        CHECK_EQ(cyc_full(&queue), hi - lo == 7);
        at = queue.front;
        for (int k = lo; k < hi; k++) {
            CHECK_EQ(queue.data[at], buf[k]);
            at = (at + 1) % queue.cap;
        }
        if (hi > lo) {
            int tail = 0;
            int index = -1;
            CHECK(cyc_tail(&queue, &tail, &index));
            CHECK_EQ(tail, buf[hi - 1]);
            CHECK(index != queue.rear);
        }
    }
}

static void test_three_modes(void) {
    CycQ cyc;
    SizeQ sized;
    TagQ tagged;
    int value = 0;
    CHECK(cyc_init(&cyc, 4));
    CHECK(size_init(&sized, 4));
    CHECK(tag_init(&tagged, 4));
    CHECK_EQ(tagged.tag, 0);
    for (int i = 1; i <= 3; i++) {
        CHECK(cyc_enq(&cyc, i));
        CHECK(size_enq(&sized, i));
        CHECK(tag_enq(&tagged, i));
    }
    CHECK(cyc_full(&cyc));
    CHECK(!size_full(&sized));
    CHECK(!tag_full(&tagged));
    CHECK(!cyc_enq(&cyc, 4));
    CHECK_EQ(cyc_count(&cyc), 3);
    CHECK(size_enq(&sized, 4));
    CHECK(tag_enq(&tagged, 4));
    CHECK(size_full(&sized));
    CHECK(tag_full(&tagged));
    CHECK_EQ(sized.front, sized.rear);
    CHECK_EQ(tagged.front, tagged.rear);
    CHECK_EQ(tag_raw(&tagged), 0);
    CHECK_EQ(tag_count(&tagged), 4);
    CHECK(tag_deq(&tagged, &value));
    CHECK_EQ(value, 1);
    CHECK_EQ(tagged.tag, 0);
    CHECK(!tag_full(&tagged));
    CHECK_EQ(tag_count(&tagged), 3);
}

static void test_link_last(void) {
    LinkQ queue;
    int kept = 7;
    CHECK(link_init(&queue));
    CHECK(link_empty(&queue));
    CHECK(queue.front == queue.rear);
    CHECK(!link_deq(&queue, &kept));
    CHECK_EQ(kept, 7);
    CHECK(link_enq(&queue, 1));
    CHECK(link_deq(&queue, &kept));
    CHECK_EQ(kept, 1);
    CHECK(queue.front == queue.rear);
    CHECK(queue.front->next == NULL);
    CHECK(link_enq(&queue, 2));
    CHECK(link_enq(&queue, 3));
    CHECK_EQ(link_count(&queue), 2);
    CHECK(link_deq(&queue, &kept));
    CHECK_EQ(kept, 2);
    CHECK_EQ(queue.rear->data, 3);
    CHECK(link_deq(&queue, &kept));
    CHECK_EQ(kept, 3);
    CHECK(link_empty(&queue));
    link_destroy(&queue);
}

static void test_tail_2011(void) {
    TailQ queue;
    CHECK(tail_init(&queue, 6));
    CHECK_EQ(queue.front, 0);
    CHECK_EQ(queue.rear, 5);
    CHECK(tail_empty(&queue));
    CHECK(!tail_full(&queue));
    CHECK(tail_enq(&queue, 42));
    CHECK_EQ(queue.rear, 0);
    CHECK_EQ(queue.front, 0);
    CHECK_EQ(queue.data[0], 42);
    CHECK_EQ(tail_count(&queue), 1);
    CHECK(!tail_init(&queue, 1));
    queue.cap = 8;
    queue.front = 3;
    queue.rear = 1;
    CHECK(tail_full(&queue));
    CHECK_EQ(tail_count(&queue), 7);
    queue.front = 0;
    queue.rear = 5;
    queue.cap = 6;
    CHECK(tail_empty(&queue));
    CHECK_EQ(tail_count(&queue), 0);
}

static void expect_seq(int n, const int *seq, int in_ok, int out_ok, int batch_ok) {
    CHECK_EQ(deque_in_check(n, seq, NULL), in_ok);
    CHECK_EQ(deque_out_check(n, seq, NULL), out_ok);
    CHECK_EQ(deque_batch_check(n, seq, NULL), batch_ok);
}

static void test_deque_known(void) {
    const int n3_123[] = {1, 2, 3};
    const int n3_132[] = {1, 3, 2};
    const int n3_213[] = {2, 1, 3};
    const int n3_231[] = {2, 3, 1};
    const int n3_312[] = {3, 1, 2};
    const int n3_321[] = {3, 2, 1};
    const int both[] = {1, 2, 3, 4};
    const int only_in[] = {4, 1, 3, 2};
    const int only_out[] = {4, 2, 1, 3};
    const int neither[] = {4, 2, 3, 1};
    const int y2010_a[] = {2, 1, 3, 4, 5};
    const int y2010_b[] = {4, 2, 1, 3, 5};
    const int y2010_c[] = {4, 2, 3, 1, 5};
    const int y2010_d[] = {5, 3, 2, 1, 4};
    const int y2021_a[] = {5, 4, 3, 1, 2};
    const int y2021_b[] = {5, 3, 1, 2, 4};
    const int y2021_c[] = {4, 2, 1, 3, 5};
    const int y2021_d[] = {4, 1, 3, 2, 5};
    expect_seq(3, n3_123, 1, 1, 1);
    expect_seq(3, n3_132, 1, 1, 0);
    expect_seq(3, n3_213, 1, 1, 1);
    expect_seq(3, n3_231, 1, 1, 0);
    expect_seq(3, n3_312, 1, 1, 1);
    expect_seq(3, n3_321, 1, 1, 1);
    expect_seq(4, both, 1, 1, 1);
    expect_seq(4, only_in, 1, 0, 0);
    expect_seq(4, only_out, 0, 1, 1);
    expect_seq(4, neither, 0, 0, 0);
    expect_seq(5, y2010_a, 1, 1, 1);
    expect_seq(5, y2010_b, 0, 1, 1);
    expect_seq(5, y2010_c, 0, 0, 0);
    expect_seq(5, y2010_d, 0, 1, 1);
    expect_seq(5, y2021_a, 1, 1, 1);
    expect_seq(5, y2021_b, 0, 1, 1);
    expect_seq(5, y2021_c, 0, 1, 1);
    expect_seq(5, y2021_d, 1, 0, 0);
    CHECK(!deque_batch_check(0, both, NULL));
    CHECK_EQ(deque_enum(6, stdout), -1);
}

static void walk_perms(int n, int *used, int *cur, int len, int *sum, int *batch) {
    if (len == n) {
        int out_ok = deque_out_check(n, cur, NULL);
        int batch_ok = deque_batch_check(n, cur, NULL);
        if (batch_ok) {
            CHECK(out_ok);
            (*batch)++;
        }
        (*sum)++;
        return;
    }
    for (int v = 1; v <= n; v++) {
        if (used[v]) {
            continue;
        }
        used[v] = 1;
        cur[len] = v;
        walk_perms(n, used, cur, len + 1, sum, batch);
        used[v] = 0;
    }
}

static void test_batch_count(void) {
    int used[6] = {0};
    int cur[5];
    int sum = 0;
    int batch = 0;
    walk_perms(4, used, cur, 0, &sum, &batch);
    CHECK_EQ(sum, 24);
    CHECK_EQ(batch, 8);
}

static void check_ring(const EndsQ *Q, const int *buf, int n) {
    int at = Q->front;
    CHECK_EQ(ends_count(Q), n);
    CHECK_EQ(ends_empty(Q), n == 0);
    for (int i = 0; i < n; i++) {
        CHECK_EQ(Q->data[at], buf[i]);
        at = (at + 1) % Q->cap;
    }
    if (n > 0) {
        CHECK_EQ(at, Q->rear);
    }
}

static void test_ends_model(void) {
    EndsQ queue;
    int buf[8];
    int n = 0;
    int cap = 6;
    CHECK(ends_init(&queue, cap));
    for (int i = 0; i < 60; i++) {
        int full = n == cap - 1;
        int op = i % 4;
        int value = i + 1;
        if ((op == 0 || n == 0) && !full) {
            CHECK(ends_enq_rear(&queue, value));
            buf[n++] = value;
        } else if (op == 1 && !full) {
            CHECK(ends_enq_front(&queue, value));
            memmove(buf + 1, buf, (size_t)n * sizeof(buf[0]));
            buf[0] = value;
            n++;
        } else if (op == 2 && n > 0) {
            int got = 0;
            CHECK(ends_deq_front(&queue, &got));
            CHECK_EQ(got, buf[0]);
            memmove(buf, buf + 1, (size_t)(n - 1) * sizeof(buf[0]));
            n--;
        } else if (n > 0) {
            int got = 0;
            CHECK(ends_deq_rear(&queue, &got));
            CHECK_EQ(got, buf[n - 1]);
            n--;
        }
        check_ring(&queue, buf, n);
        CHECK_EQ(ends_full(&queue), n == cap - 1);
    }
}

static void test_ends_2014(void) {
    EndsQ queue;
    EndsJudge judge;
    int kept = 3;
    CHECK(ends_judge(6, 0, 0, &judge));
    CHECK_EQ(judge.real_empty, 1);
    CHECK_EQ(judge.real_full, 0);
    CHECK_EQ(judge.a_empty, 1);
    CHECK_EQ(judge.a_full, 0);
    CHECK_EQ(judge.c_empty, 0);
    CHECK_EQ(judge.d_empty, 0);
    CHECK(ends_judge(6, 0, 5, &judge));
    CHECK_EQ(judge.real_full, 1);
    CHECK_EQ(judge.a_full, 1);
    CHECK_EQ(judge.b_full, 0);
    CHECK_EQ(judge.d_full, 0);
    CHECK(ends_init(&queue, 6));
    for (int i = 1; i <= 5; i++) {
        CHECK(ends_enq_rear(&queue, i));
    }
    CHECK(ends_full(&queue));
    CHECK_EQ(queue.front, 0);
    CHECK_EQ(queue.rear, 5);
    CHECK(!ends_enq_front(&queue, 9));
    CHECK(!ends_enq_rear(&queue, 9));
    CHECK_EQ(queue.rear, 5);
    CHECK(ends_deq_front(&queue, &kept));
    CHECK_EQ(kept, 1);
    CHECK(!ends_full(&queue));
    CHECK(!ends_judge(1, 0, 0, &judge));
}

int main(void) {
    test_formula();
    test_cyc_oracle();
    test_three_modes();
    test_link_last();
    test_tail_2011();
    test_deque_known();
    test_batch_count();
    test_ends_model();
    test_ends_2014();
    return TEST_SUMMARY();
}
