#include <stdio.h>
#include <stdlib.h>

#include "prompt.h"
#include "majority_element.h"
#include "seqlist.h"
#include "two_median.h"

int main(void) {
    int n =0;
    prompt("请输入序列长度 n:");
    if (scanf("%d", &n)!=1|| n <1)
        return 1;

    int *a = (int *)malloc(sizeof(int) * n);
    int *b = (int *)malloc(sizeof(int) * n);

    prompt("请输入序列 A 的 %d 个整数：",n);
    for (int i=0;i<n;i++)
        scanf("%d",&a[i]);

    prompt("请输入序列 B 的 %d 个整数：",n);
    for (int i=0;i<n;i++)
        scanf("%d",&b[i]);


    int ans=two_median(a,b,n,stdout);
    printf("median=%d\n",ans);

    free(a);
    free(b);



    return 0;
}
