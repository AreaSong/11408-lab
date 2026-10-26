#include "prompt.h"
#include "smtp.h"

#include <stdio.h>
#include <string.h>

static void strip(char *s) {
    size_t n = strlen(s);

    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[--n] = '\0';
    }
}

int main(void) {
    Smtp box;
    char line[1200];
    char resp[160];

    smtp_init(&box);
    smtp_greet(resp, (int)sizeof resp);
    puts(resp);
    while (1) {
        prompt("请输入 SMTP 命令（HELO、MAIL FROM、RCPT TO、DATA、QUIT）：");
        if (fgets(line, (int)sizeof line, stdin) == NULL) {
            break;
        }
        if (strchr(line, '\n') == NULL && !feof(stdin)) {
            fprintf(stderr, "这一行太长\n");
            return 1;
        }
        strip(line);
        smtp_cmd(&box, line, resp, (int)sizeof resp);
        if (resp[0] != '\0') {
            puts(resp);
        }
        if (smtp_closed(&box)) {
            break;
        }
    }
    if (smtp_count(&box) > 0) {
        fputc('\n', stdout);
        smtp_write(&box, stdout);
    }
    return 0;
}
