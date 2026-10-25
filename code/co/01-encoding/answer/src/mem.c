#include "mem.h"

#include "bits.h"
#include "code.h"
#include "number.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void set_err(char *err, size_t n, const char *msg) {
    if (err != NULL && n > 0) {
        snprintf(err, n, "%s", msg);
    }
}

static bool lookup_type(const char *name, int *size, int *align) {
    static const struct {
        const char *name;
        int size;
        int align;
    } tab[] = {
        {"char", 1, 1}, {"short", 2, 2}, {"int", 4, 4},
        {"float", 4, 4}, {"long", 4, 4}, {"double", 8, 8},
    };
    size_t i;
    for (i = 0; i < sizeof tab / sizeof tab[0]; i++) {
        if (strcmp(name, tab[i].name) == 0) {
            *size = tab[i].size;
            *align = tab[i].align;
            return true;
        }
    }
    return false;
}

static bool looks_machine(const char *s) {
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        return true;
    }
    if (s[0] != '\0' && (s[strlen(s) - 1] == 'H' || s[strlen(s) - 1] == 'h')) {
        return true;
    }
    for (; *s != '\0'; s++) {
        if ((*s >= 'A' && *s <= 'F') || (*s >= 'a' && *s <= 'f')) {
            return true;
        }
    }
    return false;
}

static bool encode_value(const char *type, int size, const char *text, unsigned long long *machine, char *err,
                         size_t errn) {
    Decimal d;
    long long value = 0;
    Bits bits;
    if (looks_machine(text)) {
        unsigned long long raw = 0;
        if (!bits_parse_hex(text, &raw, err, errn)) {
            return false;
        }
        if (size < 8 && raw > bits_mask(size * 8)) {
            snprintf(err, errn, "机器数超出 %d 字节", size);
            return false;
        }
        *machine = raw;
        return true;
    }
    if (strcmp(type, "float") == 0 || strcmp(type, "double") == 0 || size > 4) {
        set_err(err, errn, "这个成员请写带 H 的机器数；浮点十进制编码在第 04 关");
        return false;
    }
    if (!decimal_parse(text, &d, err, errn) || !decimal_is_int(&d) || !decimal_to_i64(&d, &value)) {
        set_err(err, errn, "成员的值要是整数真值，或带 H 的机器数");
        return false;
    }
    if (!code_encode(size * 8, value, KIND_COMP, &bits)) {
        snprintf(err, errn, "真值 %lld 超出 %d 位补码", value, size * 8);
        return false;
    }
    *machine = bits.bits;
    return true;
}

static bool read_count(const char *tok, int *i, int *count, char *err, size_t errn) {
    *count = 0;
    (*i)++;
    if (!isdigit((unsigned char)tok[*i])) {
        set_err(err, errn, "数组长度要是正整数");
        return false;
    }
    while (isdigit((unsigned char)tok[*i])) {
        *count = *count * 10 + (tok[*i] - '0');
        if (*count > 1024) {
            set_err(err, errn, "数组太长");
            return false;
        }
        (*i)++;
    }
    if (tok[*i] != ']' || *count < 1) {
        set_err(err, errn, "数组写成 name[10] 这种样子");
        return false;
    }
    (*i)++;
    return true;
}

static bool parse_decl(const char *tok, Member *m, int elem, char *value, size_t value_n, char *err, size_t errn) {
    int i = 0;
    int k = 0;
    int count = 1;
    if (!(isalpha((unsigned char)tok[0]) || tok[0] == '_')) {
        set_err(err, errn, "成员名要以字母或下划线开头");
        return false;
    }
    while (isalnum((unsigned char)tok[i]) || tok[i] == '_') {
        if (k + 1 >= MEM_NAME) {
            set_err(err, errn, "成员名太长");
            return false;
        }
        m->name[k++] = tok[i++];
    }
    m->name[k] = '\0';
    if (tok[i] == '[' && !read_count(tok, &i, &count, err, errn)) {
        return false;
    }
    m->size = elem * count;
    if (count > 1) {
        snprintf(m->label, sizeof m->label, "%s[%d]", m->type_name, count);
    }
    if (tok[i] == '=') {
        snprintf(value, value_n, "%s", tok + i + 1);
        if (value[0] == '\0') {
            set_err(err, errn, "等号右边空着");
            return false;
        }
        m->has_value = true;
        return true;
    }
    if (tok[i] != '\0') {
        set_err(err, errn, "成员写成「int a=273」或「char name[10]」，等号两边不要空格");
        return false;
    }
    return true;
}

static bool split_type(char *buf, char **type, char **decl, bool *is_end, char *err, size_t errn) {
    char *p = buf;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\0' || *p == '\n') {
        set_err(err, errn, "空行");
        return false;
    }
    *type = p;
    while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n') {
        p++;
    }
    if (*p != '\0') {
        *p++ = '\0';
        while (*p == ' ' || *p == '\t') {
            p++;
        }
    }
    *decl = p;
    (*decl)[strcspn(*decl, "\r\n")] = '\0';
    if (strcmp(*type, "end") == 0 && **decl == '\0') {
        *is_end = true;
    }
    return true;
}

bool mem_parse_member(const char *line, Member *m, bool *is_end, char *err, size_t errn) {
    char buf[160];
    char value[80];
    char *type = NULL;
    char *decl = NULL;
    int elem_size = 0;
    int elem_align = 0;
    *is_end = false;
    if (err != NULL && errn > 0) {
        err[0] = '\0';
    }
    snprintf(buf, sizeof buf, "%s", line);
    if (!split_type(buf, &type, &decl, is_end, err, errn)) {
        return false;
    }
    if (*is_end) {
        return true;
    }
    if (!lookup_type(type, &elem_size, &elem_align)) {
        set_err(err, errn, "类型用 char、short、int、float、long、double");
        return false;
    }
    memset(m, 0, sizeof *m);
    snprintf(m->type_name, sizeof m->type_name, "%s", type);
    snprintf(m->label, sizeof m->label, "%s", type);
    m->align = elem_align;
    if (decl[0] == '\0') {
        set_err(err, errn, "缺成员名");
        return false;
    }
    if (!parse_decl(decl, m, elem_size, value, sizeof value, err, errn)) {
        return false;
    }
    if (!m->has_value) {
        return true;
    }
    if (m->size > 8) {
        set_err(err, errn, "超过 8 字节的成员只算偏移，不要赋值");
        return false;
    }
    return encode_value(m->type_name, m->size, value, &m->machine, err, errn);
}

int mem_layout(Member *m, int count, int *max_align, int *total) {
    int offset = 0;
    int align = 1;
    int i;
    for (i = 0; i < count; i++) {
        int rem = offset % m[i].align;
        m[i].pad_before = rem == 0 ? 0 : m[i].align - rem;
        offset += m[i].pad_before;
        m[i].offset = offset;
        offset += m[i].size;
        if (m[i].align > align) {
            align = m[i].align;
        }
    }
    if (offset % align != 0) {
        offset += align - (offset % align);
    }
    *max_align = align;
    *total = offset;
    return 0;
}

int mem_byte(unsigned long long machine, int size, bool little, int index) {
    int shift = little ? index * 8 : (size - 1 - index) * 8;
    return (int)((machine >> shift) & 0xFF);
}
