#ifndef TRANS_H
#define TRANS_H

enum { FLAT_OK = 0, FLAT_BOUND = 1, FLAT_FAULT = 2 };
enum { TWO_OK = 0, TWO_FAULT_DIR = 1, TWO_FAULT_PAGE = 2 };
enum { LOOK_TLB = 0, LOOK_PT = 1, LOOK_FAULT = 2 };

typedef struct {
    int result;
    unsigned long long pte;
    unsigned long long pa;
} FlatInfo;

typedef struct {
    int result;
    unsigned long long pde;
    unsigned long long pt_base;
    unsigned long long pte;
    unsigned long long pa;
} TwoInfo;

typedef struct {
    int result;
    unsigned long long frame;
    unsigned long long pa;
} LookInfo;

int trans_flat(unsigned long long page_bytes, unsigned long long pte_bytes, unsigned long long length,
               unsigned long long base, unsigned long long page, int valid, unsigned long long frame,
               unsigned long long offset, FlatInfo *info);
int trans_two(unsigned long long page_bytes, unsigned long long pte_bytes, unsigned long long dir_base,
              unsigned long long dir, int dir_valid, unsigned long long pt_frame, unsigned long long page,
              int page_valid, unsigned long long frame, unsigned long long offset, TwoInfo *info);
int trans_look(unsigned long long page_bytes, unsigned long long page, unsigned long long offset,
               int tlb_valid, unsigned long long tlb_page, unsigned long long tlb_frame, int pt_valid,
               unsigned long long pt_frame, LookInfo *info);

#endif
