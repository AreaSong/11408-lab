#ifndef SCAN_H
#define SCAN_H

/* 返回 1 取到一项，0 没有了，-1 这一项超过缓冲区。 */
int take_token(char **p, char *out, int cap);
int take_need(char **p, char *out, int cap, const char *what);
int rest_blank(const char *p);
int reject(const char *msg);
int command_loop(const char *hint, int (*run)(char *line));

int parse_u32(const char *s, unsigned int *out);
int parse_ip(const char *s, unsigned char out[4]);
int parse_port(const char *s, unsigned short *out);
int parse_hex(const char *s, unsigned char *out, int cap, int *n);
int parse_bits16(const char *s, unsigned short *out);

#endif
