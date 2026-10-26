#ifndef FTPSESS_H
#define FTPSESS_H

#define FTP_XFER_MAX 8

enum { FTP_LIST = 1, FTP_GET = 2, FTP_PUT = 3 };

typedef struct {
    const char *family;
    const char *id;
    int holds;
    const char *title;
    const char *why;
} FtpNote;

typedef struct {
    int passive;
    int mode_set;
    int logged;
    int quit;
    int n;
    int kind[FTP_XFER_MAX];
    char name[FTP_XFER_MAX][48];
} FtpSess;

const FtpNote *ftp_note(const char *family, const char *id);
void ftp_init(FtpSess *s);
const char *ftp_mode(FtpSess *s, int passive);
const char *ftp_login(FtpSess *s);
const char *ftp_xfer(FtpSess *s, int kind, const char *name);
const char *ftp_quit(FtpSess *s);

#endif
