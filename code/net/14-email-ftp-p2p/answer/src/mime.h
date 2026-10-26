#ifndef MIME_H
#define MIME_H

/* encoded 为 1 时正文用了 Base64。b64 是正文的编码，7bit 时为空串。 */
typedef struct {
    const char *from;
    const char *to;
    const char *subject;
    const unsigned char *body;
    int body_len;
    const char *type;
} MimeIn;

typedef struct {
    char b64[1200];
    int encoded;
    int subject_encoded;
} MimeInfo;

int mime_needs_b64(const unsigned char *p, int n);
int mime_build(const MimeIn *in, char *out, int cap, MimeInfo *info);

#endif
