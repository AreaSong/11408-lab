#include "recur.h"

#include "sqstack.h"

long long fact_loop(int n) {
    long long value = 1;
    if (n < 0 || n > FACT_MAX) {
        return -1;
    }
    for (int i = 2; i <= n; i++) {
        value *= i;
    }
    return value;
}

static void push_calls(SqStack *S, int n, FILE *out, int *max_depth) {
    int bottom = (n == 0) ? 0 : 1;
    for (int cur = n; cur >= bottom; cur--) {
        int depth = 0;
        sq_push(S, cur);
        depth = sq_size(S);
        if (depth > *max_depth) {
            *max_depth = depth;
        }
        if (out != NULL) {
            fprintf(out, "call n=%d depth=%d stack=", cur, depth);
            sq_print(S, out);
            fputc('\n', out);
        }
    }
}

static long long pop_rets(SqStack *S, FILE *out) {
    long long value = 1;
    while (!sq_empty(S)) {
        int k = 0;
        sq_pop(S, &k);
        /* n<=1 是递归出口，返回 1。更大的 n 要乘上已经算好的 fact(n-1)。 */
        if (k > 1) {
            value *= k;
        }
        if (out != NULL) {
            fprintf(out, "ret n=%d value=%lld depth=%d stack=", k, value, sq_size(S));
            sq_print(S, out);
            fputc('\n', out);
        }
    }
    return value;
}

bool fact_by_stack(int n, FILE *out, FactResult *result) {
    SqStack stack;
    int max_depth = 0;
    if (result == NULL || n < 0 || n > FACT_MAX) {
        return false;
    }
    if (!sq_init(&stack, n == 0 ? 1 : n)) {
        return false;
    }
    /* 阶乘在返回之前只有一次子调用，所以可以先把各层参数都压进去，再倒着乘。
     * 真要改循环时不必用栈；这里留着栈，是为了看见递归有多深。 */
    push_calls(&stack, n, out, &max_depth);
    result->value = pop_rets(&stack, out);
    result->max_depth = max_depth;
    result->ok = true;
    if (out != NULL) {
        fprintf(out, "max_depth=%d result=%lld\n", max_depth, result->value);
    }
    return true;
}
