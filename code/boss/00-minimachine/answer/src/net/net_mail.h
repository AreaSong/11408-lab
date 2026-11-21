#ifndef NET_MAIL_H
#define NET_MAIL_H

int net_b64_encode(const unsigned char *in, int n, char *out, int cap);
int net_mail_line(const char *id, char *out, int cap);
int net_ftp_line(const char *id, char *out, int cap);
int net_p2p_line(const char *id, char *out, int cap);
int net_aexam_line(const char *id, char *out, int cap);

#endif
