#include <stdio.h>

/*
 * diskutil 的分区表随这台机器变，不进 tests/。
 * 低级格式化划扇区，分区写分区表，高级格式化建空文件系统。这三步和本机输出对看。
 */
int main(void) {
    FILE *pipe;
    char line[512];

    printf("legend 1=physical-format sector 2=partition table 3=logical-format filesystem\n");
    printf("note exam-model=bios-mbr this-mac-may-differ\n");
    pipe = popen("diskutil list", "r");
    if (pipe == NULL) {
        perror("diskutil");
        return 1;
    }
    while (fgets(line, (int)sizeof line, pipe) != NULL) {
        fputs(line, stdout);
    }
    return pclose(pipe) == 0 ? 0 : 1;
}
