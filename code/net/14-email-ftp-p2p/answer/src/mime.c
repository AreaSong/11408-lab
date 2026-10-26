#include "mime.h"

#include "b64.h"

#include <stdio.h>
#include <string.h>

int mime_needs_b64(const unsigned char *p, int n) {
    int i;

    for (i = 0; i < n; i++) {
        unsigned char c = p[i];
        if (c == '\n' || c == '\r' || c == '\t') {
            continue;
        }
        if (c < 32 || c > 126) {
            return 1;
        }
    }
    return 0;
}

static int app_n(char **p, int *left, const char *s, int n) {
    if (n >= *left) {
        return -1;
    }
    memcpy(*p, s, (size_t)n);
    *p += n;
    *left -= n;
    **p = '\0';
    return 0;
}

static int app(char **p, int *left, const char *s) {
    return app_n(p, left, s, (int)strlen(s));
}

static int appf(char **p, int *left, const char *fmt, const char *a) {
    char line[180];
    int n = snprintf(line, sizeof line, fmt, a);

    if (n < 0 || n >= (int)sizeof line) {
        return -1;
    }
    return app(p, left, line);
}

static int subject_text(const MimeIn *in, char *buf, int cap, int *encoded) {
    if (!mime_needs_b64((const unsigned char *)in->subject, (int)strlen(in->subject))) {
        *encoded = 0;
        snprintf(buf, (size_t)cap, "%s", in->subject);
        return 0;
    }
    *encoded = 1;
    if (b64_encode((const unsigned char *)in->subject, (int)strlen(in->subject), buf, cap) < 0) {
        return -1;
    }
    return 0;
}

static int write_subject(char **p, int *left, const char *text, int encoded) {
    char line[1600];
    int n;

    if (encoded) {
        n = snprintf(line, sizeof line, "Subject: =?UTF-8?B?%s?=\n", text);
    } else {
        n = snprintf(line, sizeof line, "Subject: %s\n", text);
    }
    if (n < 0 || n >= (int)sizeof line) {
        return -1;
    }
    return app(p, left, line);
}

int mime_build(const MimeIn *in, char *out, int cap, MimeInfo *info) {
    char sub[1400];
    char *p = out;
    int left = cap;
    const char *charset = "us-ascii";
    const char *coding = "7bit";

    if (in == NULL || out == NULL || info == NULL || cap <= 0) {
        return -1;
    }
    memset(info, 0, sizeof *info);
    if (subject_text(in, sub, (int)sizeof sub, &info->subject_encoded) != 0) {
        return -1;
    }
    info->encoded = mime_needs_b64(in->body, in->body_len);
    if (info->encoded) {
        if (b64_encode(in->body, in->body_len, info->b64, (int)sizeof info->b64) < 0) {
            return -1;
        }
        charset = "UTF-8";
        coding = "base64";
    }
    if (appf(&p, &left, "From: %s\n", in->from) != 0 || appf(&p, &left, "To: %s\n", in->to) != 0 ||
        write_subject(&p, &left, sub, info->subject_encoded) != 0 || app(&p, &left, "MIME-Version: 1.0\n") != 0) {
        return -1;
    }
    if (in->type != NULL && in->type[0] != '\0') {
        if (appf(&p, &left, "Content-Type: %s\n", in->type) != 0) {
            return -1;
        }
    } else if (appf(&p, &left, "Content-Type: text/plain; charset=%s\n", charset) != 0) {
        return -1;
    }
    if (appf(&p, &left, "Content-Transfer-Encoding: %s\n\n", coding) != 0) {
        return -1;
    }
    if (info->encoded) {
        return app(&p, &left, info->b64) != 0 || app(&p, &left, "\n") != 0 ? -1 : 0;
    }
    if (in->body_len > 0 && app_n(&p, &left, (const char *)in->body, in->body_len) != 0) {
        return -1;
    }
    if (in->body_len == 0 || in->body[in->body_len - 1] != '\n') {
        return app(&p, &left, "\n");
    }
    return 0;
}
