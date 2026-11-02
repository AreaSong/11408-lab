#include <stdio.h>

#include "prompt.h"
#include "segments.h"

int main(void) {
    char id[32];
    int n=0;

    for (;;) {
        prompt("请输入「编号 n」，例如2012-1 100 （Ctrl + D结束)：");

        if (scanf("%31s %d",id,&n)!=2) {
            break;
        }

        long long count=0;
        if (!segment_count(id,n,&count)) {
            printf("%s unknown\n",id);
            continue;
        }

        printf("%s n=%d count=%lld\n",id ,n ,count);
    }

    return 0;
}
