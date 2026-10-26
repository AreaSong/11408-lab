#ifndef ADDR_H
#define ADDR_H

int addr_cut(const int *widths, int n, unsigned long long addr, unsigned long long *parts);
int addr_pack(const int *widths, const unsigned long long *values, int n, unsigned long long *addr);
int addr_slot(unsigned long long pte_bytes, unsigned long long base, unsigned long long index,
              unsigned long long *out);
int addr_join(unsigned long long page_bytes, unsigned long long frame, unsigned long long offset,
              unsigned long long *pa);
int addr_elem(unsigned long long start, unsigned long long cols, unsigned long long elem_bytes,
              unsigned long long row, unsigned long long col, unsigned long long *index,
              unsigned long long *addr);
int span_count(unsigned long long page_bytes, unsigned long long length, int *count);
int span_at(unsigned long long page_bytes, unsigned long long pte_bytes, unsigned long long la,
            unsigned long long load_pa, unsigned long long table, int index, unsigned long long *page_no,
            unsigned long long *pte_addr, unsigned long long *frame, unsigned long long *pa);
int addr_frag(unsigned long long page_bytes, unsigned long long bytes, unsigned long long *pages,
              unsigned long long *internal);

#endif
