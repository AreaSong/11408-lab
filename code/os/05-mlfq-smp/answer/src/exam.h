#ifndef EXAM_H
#define EXAM_H

#include <stdbool.h>

const char *exam_card(int index);
bool exam_row(const char *kind, const char *name, const char **line);
bool exam_claim(const char *name, const char **truth);

#endif
