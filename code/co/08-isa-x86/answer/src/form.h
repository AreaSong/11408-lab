#ifndef FORM_H
#define FORM_H

#include <stdio.h>

/* naddr 为 0～4。四地址多出来的字段是下一条指令的地址，PC 出现以后就不用了。 */
int show_kind(FILE *out, int naddr);

#endif
