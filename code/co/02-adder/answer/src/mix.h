#ifndef MIX_H
#define MIX_H

#include "alu.h"

/*
 * 卷面模型：char 8 位、short 16 位、int 32 位。
 * int 装得下 unsigned short 的全部取值，所以 unsigned short 先提升成 int。
 * int 和 unsigned 等级相同，混在一起算时 int 转成 unsigned。
 */

typedef enum {
    TY_CHAR = 0,
    TY_UCHAR,
    TY_SHORT,
    TY_USHORT,
    TY_INT,
    TY_UINT
} CType;

typedef enum {
    MIX_CMP = 0,
    MIX_ADD,
    MIX_SUB
} MixOp;

typedef struct {
    CType type;
    int width;
    int is_unsigned;
    unsigned long long bits;
    long long sval;
    unsigned long long uval;
} CVal;

typedef struct {
    MixOp op;
    CVal left;
    CVal right;
    CVal left_promoted;
    CVal right_promoted;
    CVal left_common;
    CVal right_common;
    CType common;
    int rel;
    AluResult alu;
} MixResult;

typedef struct {
    int infinite;
    int entered;
    Word before;
    Word after;
} WrapInfo;

typedef struct {
    int fits;
    Word stored;
} FitInfo;

bool ctype_parse(const char *text, CType *out);
const char *ctype_name(CType type);

bool mix_eval(MixOp op, CType left_type, const char *left_text, CType right_type,
              const char *right_text, MixResult *out, char *err, size_t errn);
bool wrap_eval(int is_unsigned, long long n, WrapInfo *out, char *err, size_t errn);
bool fit_eval(int is_unsigned, long long value, FitInfo *out, char *err, size_t errn);
bool parse_i64(const char *text, long long *out);

int mix_print(FILE *out, const MixResult *m);
int wrap_print(FILE *out, const WrapInfo *info, int is_unsigned);
int fit_print(FILE *out, const FitInfo *info, int is_unsigned, long long value);
int read_print(FILE *out, int as_signed, Word w);

#endif
