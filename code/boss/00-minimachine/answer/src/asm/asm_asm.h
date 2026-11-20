#ifndef ASM_ASM_H
#define ASM_ASM_H

#include <stddef.h>
#include <stdint.h>

typedef struct AsmImage AsmImage;

/* 两遍汇编。成功返回 0，并让 *out 指向新映像；失败返回 -1，错误写进 err。 */
int asm_assemble(const char *src, AsmImage **out, char *err, size_t errcap);
void asm_free(AsmImage *img);

const char *asm_report(const AsmImage *img);
uint32_t asm_entry(const AsmImage *img);
int asm_symbol(const AsmImage *img, const char *name, uint32_t *addr);

/*
 * 汇编一条没有标号表的语句（反汇编往返、word 命令用）。
 * words 至少能放 2 个字。伪指令 li/push/pop 可能得到 2 个字，la 需要标号，这里会失败。
 */
int asm_encode_line(const char *line, uint32_t pc, uint32_t *words, int *nwords,
                    char *err, size_t errcap);

#endif
