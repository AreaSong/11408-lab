#ifndef PROTO_H
#define PROTO_H

#include <stdio.h>

#define PROTO_MAX 8

typedef struct {
    const char *family;
    const char *id;
    int holds;
    int udp;
    const char *title;
    const char *why;
} ProtoRule;

typedef struct {
    char family[16];
    const ProtoRule *item[PROTO_MAX];
    char extra[PROTO_MAX][16];
    int n;
} ProtoQuiz;

const ProtoRule *proto_find(const char *family, const char *id);
const char *proto_add(ProtoQuiz *q, const char *family, const char *id, const char *extra);
void proto_show(const ProtoQuiz *q, FILE *fp);

#endif
