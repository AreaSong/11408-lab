#include <stdio.h>

#include "prompt.h"
#include "majority_element.h"
#include "seqlist.h"
#include "two_median.h"


static long long count_inserts(SeqList *L,int n) {
    long long total=0;
    for (int i=1;i<=n+1;i++) {
        int moves=0;
        seqlist_insert(L,i,-1,&moves);
        total+=moves;
        seqlist_delete(L,i,NULL,NULL);

    }

    return total;
}

static long long count_deletes(SeqList *L,int n) {
    long long total=0;
    for (int i=1;i<=n;i++) {
        int moves=0;
        int val=0;
        seqlist_delete(L,i,&val,&moves);
        total+=moves;
        seqlist_insert(L,i,val,NULL);
    }
    return total;
}




int main(void) {
    int n=0;
    while (1) {
        prompt("请输入表长 n (1 到 20000,Ctrl+D 结束):  ");
        if (scanf("%d",&n)!=1) {
            break;
        }

        SeqList L;
        seqlist_init(&L);
        for (int i=1;i<=n;i++) {
            seqlist_insert(&L,i,i,NULL);
        }

        long long ins_total=count_inserts(&L,n);
        long long del_total=count_deletes(&L,n);

        printf("n=%d insert total=%lld trials=%d avg=%.1f expect=%.1f "
               "delete total=%lld trials=%d avg=%.1f expect=%.1f\n",
               n,
               ins_total,n+1,(double)ins_total / (n+1) , n/2.0,
               del_total,n,(double)del_total /n,(n-1)/2.0);

        seqlist_destroy(&L);


    }


    return 0;
}
