#include "bracket.h"

#include "sqstack.h"

#include <stdbool.h>

static char match_of(char left) {
    if (left == '(') {
        return ')';
    }
    if (left == '[') {
        return ']';
    }
    if (left == '{') {
        return '}';
    }
    return 0;
}

static int is_left(char ch) {
    return ch == '(' || ch == '[' || ch == '{';
}

static int is_right(char ch) {
    return ch == ')' || ch == ']' || ch == '}';
}

/* 括号本身就是方括号，外层分隔也用 []，所以非空时里面留空格：stack=[ [ ( ]。 */
static void print_br(const SqStack *S, FILE *out) {
    fputs("stack=[", out);
    if (!sq_empty(S)) {
        for (int i = 0; i <= S->top; i++) {
            fprintf(out, " %c", (char)S->data[i]);
        }
        fputc(' ', out);
    }
    fputc(']', out);
}

static void finish(FILE *out, const BracketResult *result) {
    const char *name = "ok";
    int overflow = 0;
    if (out == NULL) {
        return;
    }
    if (result->status == BR_OVERFLOW) {
        name = "overflow";
        overflow = 1;
    } else if (result->status == BR_MISMATCH) {
        name = "mismatch";
    } else if (result->status == BR_UNCLOSED) {
        name = "unclosed";
    }
    fprintf(out, "max_depth=%d overflow=%d status=%s\n", result->max_depth, overflow, name);
}

static void on_left(SqStack *S, char ch, int cap, FILE *out, BracketResult *result, int *stop) {
    int need = sq_size(S) + 1;
    if (need > result->max_depth) {
        result->max_depth = need;
    }
    if (sq_full(S)) {
        result->status = BR_OVERFLOW;
        if (out != NULL) {
            fprintf(out, "scan=%c reject depth=%d cap=%d\n", ch, need, cap);
        }
        *stop = 1;
        return;
    }
    sq_push(S, (unsigned char)ch);
    if (out != NULL) {
        fprintf(out, "scan=%c push ", ch);
        print_br(S, out);
        fprintf(out, " depth=%d\n", sq_size(S));
    }
}

static void on_right(SqStack *S, char ch, FILE *out, BracketResult *result, int *stop) {
    int topv = 0;
    int empty = sq_empty(S);
    if (!empty) {
        sq_top(S, &topv);
    }
    if (empty || match_of((char)topv) != ch) {
        result->status = BR_MISMATCH;
        if (out != NULL && empty) {
            fprintf(out, "scan=%c mismatch top=empty\n", ch);
        } else if (out != NULL) {
            fprintf(out, "scan=%c mismatch top=%c expect=%c\n", ch, (char)topv, match_of((char)topv));
        }
        *stop = 1;
        return;
    }
    sq_pop(S, &topv);
    if (out != NULL) {
        fprintf(out, "scan=%c pop=%c ", ch, (char)topv);
        print_br(S, out);
        fprintf(out, " depth=%d\n", sq_size(S));
    }
}

bool bracket_check(const char *expr, int cap, FILE *out, BracketResult *result) {
    SqStack stack;
    int stop = 0;
    if (result == NULL || expr == NULL || cap < 1 || cap > SQ_MAX || !sq_init(&stack, cap)) {
        return false;
    }
    result->max_depth = 0;
    result->status = BR_OK;
    for (const char *p = expr; *p != '\0' && !stop; p++) {
        if (is_left(*p)) {
            on_left(&stack, *p, cap, out, result, &stop);
        } else if (is_right(*p)) {
            on_right(&stack, *p, out, result, &stop);
        }
    }
    if (!stop && !sq_empty(&stack)) {
        result->status = BR_UNCLOSED;
        if (out != NULL) {
            fputs("unclosed ", out);
            print_br(&stack, out);
            fputc('\n', out);
        }
    }
    finish(out, result);
    return true;
}
