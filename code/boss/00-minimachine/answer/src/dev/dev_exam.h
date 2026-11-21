#ifndef DEV_EXAM_H
#define DEV_EXAM_H

/* letter 得到选项。extra 为空，或是 single=/double=，或是 DMA 的顺序。失败返回 -1。 */
int dev_iexam(const char *id, char *letter, int lcap, char *extra, int ecap);
int dev_iclaim(const char *name, char *value, int cap);

#endif
