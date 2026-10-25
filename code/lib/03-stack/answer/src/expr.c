#include "expr.h"

#include "sqstack.h"

#include <string.h>

static int prec(char op) {
    if (op == '+' || op == '-') {
        return 1;
    }
    if (op == '*' || op == '/') {
        return 2;
    }
    return 0;
}

static int is_operand(char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

static int is_operator(char ch) {
    return ch == '+' || ch == '-' || ch == '*' || ch == '/';
}

static bool mark_bad(ExprInfo *info, FILE *out, const char *reason) {
    info->ok = false;
    info->reason = reason;
    if (out != NULL) {
        fprintf(out, "status=bad reason=%s\n", reason);
    }
    return false;
}

static void show_op(FILE *out, const char *scan, const char *act, const SqStack *op, const char *post) {
    if (out == NULL) {
        return;
    }
    fprintf(out, "scan=%s do=%s op=", scan, act);
    fputc('[', out);
    for (int i = 0; i <= op->top; i++) {
        if (i > 0) {
            fputc(' ', out);
        }
        fputc((char)op->data[i], out);
    }
    fputc(']', out);
    fprintf(out, " post=%s depth=%d\n", post, sq_size(op));
}

static bool push_op(SqStack *op, char ch, const char *scan, const char *post, FILE *out, ExprInfo *info) {
    if (!sq_push(op, (unsigned char)ch)) {
        return mark_bad(info, out, "overflow");
    }
    if (sq_size(op) > info->max_depth) {
        info->max_depth = sq_size(op);
    }
    show_op(out, scan, "push", op, post);
    return true;
}

static bool pop_join(SqStack *op, char *post, int *len, const char *scan, FILE *out, ExprInfo *info) {
    int value = 0;
    if (!sq_pop(op, &value)) {
        return mark_bad(info, out, "mismatch");
    }
    if (*len + 1 >= EXPR_MAX) {
        return mark_bad(info, out, "long");
    }
    post[(*len)++] = (char)value;
    post[*len] = '\0';
    show_op(out, scan, "pop", op, post);
    return true;
}

static bool drop_left(SqStack *op, const char *scan, const char *post, FILE *out, ExprInfo *info) {
    int value = 0;
    if (!sq_pop(op, &value) || (char)value != '(') {
        return mark_bad(info, out, "mismatch");
    }
    show_op(out, scan, "drop", op, post);
    return true;
}

static bool take_right(SqStack *op, char *post, int *len, FILE *out, ExprInfo *info) {
    for (;;) {
        int value = 0;
        if (sq_empty(op)) {
            return mark_bad(info, out, "mismatch");
        }
        sq_top(op, &value);
        if ((char)value == '(') {
            return drop_left(op, ")", post, out, info);
        }
        if (!pop_join(op, post, len, ")", out, info)) {
            return false;
        }
    }
}

static bool take_operator(SqStack *op, char cur, char *post, int *len, FILE *out, ExprInfo *info) {
    char scan[2] = {cur, '\0'};
    while (!sq_empty(op)) {
        int value = 0;
        char top = 0;
        sq_top(op, &value);
        top = (char)value;
        /* 左括号是这一层的底。同级运算符左边的先算，所以栈顶不低于当前时先弹。 */
        if (top == '(' || prec(top) < prec(cur)) {
            break;
        }
        if (!pop_join(op, post, len, scan, out, info)) {
            return false;
        }
    }
    return push_op(op, cur, scan, post, out, info);
}

static bool take_operand(SqStack *op, char ch, char watch, int *seen, char *post, int *len,
                         FILE *out, ExprInfo *info) {
    char scan[2] = {ch, '\0'};
    int k = 0;
    if (*len + 1 >= EXPR_MAX) {
        return mark_bad(info, out, "long");
    }
    if (watch != '\0' && ch == watch && *seen == 0) {
        for (int i = 0; i <= op->top && k + 1 < EXPR_WATCH; i++) {
            info->watch_stack[k++] = (char)op->data[i];
        }
        info->watch_stack[k] = '\0';
        *seen = 1;
    }
    post[(*len)++] = ch;
    post[*len] = '\0';
    show_op(out, scan, "out", op, post);
    return true;
}

static bool flush_ops(SqStack *op, char *post, int *len, FILE *out, ExprInfo *info) {
    while (!sq_empty(op)) {
        int value = 0;
        sq_top(op, &value);
        if ((char)value == '(') {
            return mark_bad(info, out, "unclosed");
        }
        if (!pop_join(op, post, len, "end", out, info)) {
            return false;
        }
    }
    return true;
}

static bool step(SqStack *op, char ch, char watch, int *seen, char *post, int *len, int *want_value,
                 FILE *out, ExprInfo *info) {
    char scan[2] = {ch, '\0'};
    if (is_operand(ch)) {
        if (*want_value == 0) {
            return mark_bad(info, out, "syntax");
        }
        *want_value = 0;
        return take_operand(op, ch, watch, seen, post, len, out, info);
    }
    if (ch == '(') {
        if (*want_value == 0) {
            return mark_bad(info, out, "syntax");
        }
        return push_op(op, ch, scan, post, out, info);
    }
    if (is_operator(ch)) {
        if (*want_value != 0) {
            return mark_bad(info, out, "syntax");
        }
        *want_value = 1;
        return take_operator(op, ch, post, len, out, info);
    }
    if (ch == ')') {
        if (*want_value != 0) {
            return mark_bad(info, out, "syntax");
        }
        return take_right(op, post, len, out, info);
    }
    return mark_bad(info, out, "syntax");
}

static bool convert_infix(const char *infix, char watch, FILE *out, ExprInfo *info) {
    SqStack op;
    char post[EXPR_MAX];
    int len = 0;
    int want_value = 1;
    int seen = 0;
    if (infix == NULL || infix[0] == '\0' || strlen(infix) >= EXPR_MAX) {
        return mark_bad(info, out, infix != NULL && strlen(infix) >= EXPR_MAX ? "long" : "syntax");
    }
    if (!sq_init(&op, SQ_MAX)) {
        return mark_bad(info, out, "overflow");
    }
    post[0] = '\0';
    for (const char *p = infix; *p != '\0'; p++) {
        if (!step(&op, *p, watch, &seen, post, &len, &want_value, out, info)) {
            return false;
        }
    }
    if (want_value != 0) {
        return mark_bad(info, out, "syntax");
    }
    if (!flush_ops(&op, post, &len, out, info)) {
        return false;
    }
    memcpy(info->postfix, post, (size_t)len + 1);
    return true;
}

static int postfix_numeric(const char *post) {
    if (post[0] == '\0') {
        return 0;
    }
    for (const char *p = post; *p != '\0'; p++) {
        if ((*p >= '0' && *p <= '9') || is_operator(*p)) {
            continue;
        }
        return 0;
    }
    return 1;
}

static void show_val(FILE *out, const char *scan, const char *act, const SqStack *val) {
    if (out == NULL) {
        return;
    }
    fprintf(out, "scan=%s do=%s val=", scan, act);
    sq_print(val, out);
    fprintf(out, " depth=%d\n", sq_size(val));
}

static int apply_op(char op, int left, int right, int *result) {
    if (op == '+') {
        *result = left + right;
    } else if (op == '-') {
        *result = left - right;
    } else if (op == '*') {
        *result = left * right;
    } else if (op == '/' && right != 0) {
        *result = left / right;
    } else {
        return 0;
    }
    return 1;
}

static bool eval_postfix(const char *post, FILE *out, ExprInfo *info) {
    SqStack val;
    int value = 0;
    if (!sq_init(&val, SQ_MAX)) {
        return mark_bad(info, out, "overflow");
    }
    if (out != NULL) {
        fprintf(out, "eval\n");
    }
    for (const char *p = post; *p != '\0'; p++) {
        char scan[2] = {*p, '\0'};
        int left = 0;
        int right = 0;
        int result = 0;
        if (*p >= '0' && *p <= '9') {
            if (!sq_push(&val, *p - '0')) {
                return mark_bad(info, out, "overflow");
            }
            show_val(out, scan, "push", &val);
            continue;
        }
        /* 栈顶是后进来的操作数，所以先弹出的是右操作数。 */
        if (sq_size(&val) < 2 || !sq_pop(&val, &right) || !sq_pop(&val, &left)) {
            return mark_bad(info, out, "operand");
        }
        if (!apply_op(*p, left, right, &result)) {
            return mark_bad(info, out, "div0");
        }
        if (!sq_push(&val, result)) {
            return mark_bad(info, out, "overflow");
        }
        if (out != NULL) {
            fprintf(out, "scan=%s do=op %d%c%d=%d val=", scan, left, *p, right, result);
            sq_print(&val, out);
            fprintf(out, " depth=%d\n", sq_size(&val));
        }
    }
    if (sq_size(&val) != 1 || !sq_top(&val, &value)) {
        return mark_bad(info, out, "operand");
    }
    info->value = value;
    info->has_value = true;
    return true;
}

bool expr_run(const char *infix, int do_eval, char watch, FILE *out, ExprInfo *info) {
    if (info == NULL) {
        return false;
    }
    memset(info, 0, sizeof(*info));
    if (!convert_infix(infix, watch, out, info)) {
        return false;
    }
    if (out != NULL) {
        fprintf(out, "postfix=%s\n", info->postfix);
        fprintf(out, "max_depth=%d\n", info->max_depth);
    }
    if (!do_eval) {
        info->ok = true;
        info->reason = "ok";
        if (out != NULL) {
            fprintf(out, "status=ok\n");
        }
        return true;
    }
    if (!postfix_numeric(info->postfix)) {
        return mark_bad(info, out, "need-digit");
    }
    if (!eval_postfix(info->postfix, out, info)) {
        return false;
    }
    info->ok = true;
    info->reason = "ok";
    if (out != NULL) {
        fprintf(out, "value=%lld\n", info->value);
        fprintf(out, "status=ok\n");
    }
    return true;
}
