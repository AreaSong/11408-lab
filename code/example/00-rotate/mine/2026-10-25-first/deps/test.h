#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <string.h>

typedef struct {
    int total;
    int failed;
} TestStats;

static inline  TestStats *test_stats_(void) {
    static TestStats stats= {0,0};
    return &stats;
}

static  inline void test_report_(int ok,const char *expr,const  char *file,int line) {

    TestStats *s=test_stats_();
    s->total++;
    if (!ok) {
        s->failed++;
        fprintf(stderr,"%s:%d:CHECK 失败:%s\n",file,line,expr);
    }
}

static inline  int test_summary_(void) {
    TestStats *s=test_stats_();
    if (s->failed==0) {
        printf("[PASS] %d 项检查全部通过\n",s->total);
        return 0;
    }
    printf("[FAIL] %d / %d 项检查失败\n",s->failed,s->total);
    return 1;
}

static  inline  void test_check_eq_(long long actual,long long expected, const char *expr,const char *file,int line) {
    test_report_(actual==expected,expr,file,line);
    if (actual != expected) {
        fprintf(stderr,"实际值 %lld，期望值 %lld\n",actual, expected);
    }
}

static inline void test_check_str_(const char *actual,const char *expected,const char *expr,const char *file,int line) {
    int ok = (actual == NULL || expected == NULL) ? actual == expected : strcmp(actual,expected)==0;
    test_report_(ok,expr,file,line);
    if (!ok) {
        fprintf(stderr,"实际值 \"%s\",期望值 \"%s\"\n",actual?actual:"(null)",expected?expected:"(null)");
    }
}




#define  CHECK(cond) test_report_(!!(cond),#cond,__FILE__,__LINE__)

#define CHECK_EQ(actual,expected) test_check_eq_((long long)(actual),(long long)(expected),#actual"==" #expected,__FILE__,__LINE__)

#define CHECK_STR(actual,expected) test_check_str_((actual),(expected), #actual "==" #expected,__FILE__,__LINE__)

#define TEST_SUMMARY() test_summary_()

#endif
