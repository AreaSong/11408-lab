#ifndef EXAM_H
#define EXAM_H

#include <stdbool.h>

int exam_front_count(void);
int exam_back_count(void);
const char *exam_front(int index);
const char *exam_back(int index);
bool exam_row(const char *kind, const char *name, const char **line);
bool exam_claim(const char *name, const char **truth);

#endif
