#include "two_median.h"


typedef struct {
    int s1, e1;
    int s2, e2;
} MedianSpan;

static void median_keep(MedianSpan *span, int m1, int m2, int len, int a_smaller) {
    if (a_smaller) {
        if (len % 2 == 0) {
            span->s1 = m1 + 1;
            span->e2 = m2;
        } else {
            span->s1 = m1;
            span->e2 = m2;
        }
    }else {
        if (len%2==0) {
            span->e1=m1;
            span->s2=m2+1;
        }else {
            span->e1=m1;
            span->s2=m2;
        }
    }
}

int two_median(const int a[],const int b[],int n,FILE *trace) {
    if (n<1||!a||!b)
        return 0;

    MedianSpan span={0,n-1,0,n-1};
    int round=1;
    while (span.s1<span.e1||span.s2<span.e2) {
        int m1=span.s1 + (span.e1-span.s1)/2;
        int m2=span.s2 + (span.e2-span.s2)/2;
        int len=span.e1-span.s1+1;

        if (a[m1]==b[m2]) {
            if (trace) {
                fprintf(trace,"round %d len%d A[%d..%d] mid=%d == B[%d..%d] mid=%d\n",
                     round,len,span.s1,span.e1,a[m1],span.s2,span.e2,b[m2]);
            }
            return a[m1];
        }

        int s1=span.s1,e1=span.e1;
        int s2=span.s2,e2=span.e2;
        int a_smaller = a[m1] < b[m2];

        median_keep(&span,m1,m2,len,a_smaller);

        if (trace) {
            fprintf(trace,"round %d len=%d A[%d..%d] mid=%d %c B[%d..%d] mid=%d -> A[%d..%d] B[%d..%d]\n",
                round,len,s1,e1,a[m1],a_smaller ? '<':'>',s2,e2,b[m2],span.s1,span.e1,span.s2,span.e2);
        }
        round++;
    }

    return a[span.s1] <= b[span.s2]?a[span.s1]:b[span.s2];

}
