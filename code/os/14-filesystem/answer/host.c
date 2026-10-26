#include <stdio.h>
#include <string.h>

static int dump(const char *title, const char *cmd) {
    FILE *fp;
    char line[512];

    printf("# %s\n", title);
    fp = popen(cmd, "r");
    if (fp == 0) {
        printf("result=io\n");
        return 1;
    }
    while (fgets(line, (int)sizeof line, fp) != 0) {
        fputs(line, stdout);
    }
    pclose(fp);
    return 0;
}

static void root_line(void) {
    FILE *fp = popen("mount", "r");
    char line[512];
    const char *fstype = "unknown";

    if (fp == 0) {
        printf("root=no\n");
        return;
    }
    while (fgets(line, (int)sizeof line, fp) != 0) {
        char *mark = strstr(line, " on / (");
        char *end;
        if (mark == 0) {
            continue;
        }
        mark += (int)strlen(" on / (");
        end = strchr(mark, ',');
        if (end == 0) {
            end = strchr(mark, ')');
        }
        if (end != 0 && end - mark < 32) {
            static char kind[32];
            memcpy(kind, mark, (size_t)(end - mark));
            kind[end - mark] = '\0';
            fstype = kind;
        }
        printf("root=yes\nfstype=%s\n", fstype);
        pclose(fp);
        return;
    }
    pclose(fp);
    printf("root=no\n");
}

int main(void) {
    /* 本机挂载表每次不一样，只给实验记录，不进 tests。 */
    root_line();
    if (dump("mount", "mount") || dump("df", "df -h")) {
        return 1;
    }
    return 0;
}
