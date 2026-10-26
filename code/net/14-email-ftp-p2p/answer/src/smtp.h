#ifndef SMTP_H
#define SMTP_H

#include <stdio.h>

#define SMTP_ADDR 80
#define SMTP_RCPT 4
#define SMTP_BODY 2048
#define SMTP_SAVED 4

typedef struct {
    char from[SMTP_ADDR];
    char rcpt[SMTP_RCPT][SMTP_ADDR];
    int nrcpt;
    char body[SMTP_BODY];
    int blen;
} SmtpMail;

typedef struct {
    int phase;
    int overflow;
    int nsaved;
    SmtpMail cur;
    SmtpMail saved[SMTP_SAVED];
} Smtp;

void smtp_init(Smtp *s);
void smtp_greet(char *buf, int cap);
int smtp_cmd(Smtp *s, const char *line, char *buf, int cap);
int smtp_closed(const Smtp *s);
int smtp_count(const Smtp *s);
const char *smtp_body(const Smtp *s, int index);
void smtp_write_one(const Smtp *s, int index, FILE *fp);
void smtp_write(const Smtp *s, FILE *fp);

#endif
