#include <stdio.h>
#include <string.h>

#include "prompt.h"
#include "majority_element.h"
#include "seqlist.h"
#include "two_median.h"

static void print_tail(const SeqList *L) {
    printf("length=%d cap=%d ", L->length, L->capacity);
    seqlist_print(L,stdout);
    printf("\n");
}


int main(void) {
    SeqList list;
    if (!seqlist_init(&list)) {
        fprintf(stderr, "内存分配失败\n");
        return 1;
    }

    list.trace = stdout;
    char op[16];

    while (1) {
        prompt("请输入命令 （insert / delete / find / get,Ctrl+D 结束):   ");
        if (scanf("%15s", op) != 1)
            break;

        if (strcmp(op, "insert") == 0) {
            int i = 0, e = 0;
            if (scanf("%d %d",&i,&e)!=2)
                break;
            int moves=0;
            if (seqlist_insert(&list,i,e,&moves)) {
                printf("ok insert i=%d e=%d moves=%d ",i,e,moves);
                print_tail(&list);
            }else {
                printf("fail insert i=%d e=%d\n",i,e);
            }
        }else if (strcmp(op,"delete")==0) {
            int i =0;
            if (scanf("%d",&i)!=1)
                break;
            int val=0,moves=0;

            if (seqlist_delete(&list,i,&val,&moves)) {
                printf("ok delete i=%d e=%d moves=%d ",i,val,moves);
                print_tail(&list);
            }else {
                printf("fail delete i=%d\n",i);
            }
        }else if (strcmp(op,"find")==0) {
            int e=0;
            if (scanf("%d",&e)!=1)
                break;
            printf("ok find e=%d pos=%d\n",e,seqlist_locate(&list,e));

        }else if (strcmp(op,"get")==0) {
            int i=0;
            if (scanf("%d",&i)!=1)
                break;
            int val=0;
            if (seqlist_get(&list,i,&val)) {
                printf("ok get i=%d value=%d\n",i,val);
                }
            else
                printf("fail get i=%d\n",i);
        }
    }

    seqlist_destroy(&list);
    return 0;
}
