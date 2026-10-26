#ifndef SLOT_H
#define SLOT_H

int addr_shift(unsigned long long page_bytes, int *shift);
int addr_split(unsigned long long page_bytes, unsigned long long addr, unsigned long long *page,
               unsigned long long *offset);
int pte_addr(unsigned long long page_bytes, unsigned long long pte_bytes, unsigned long long base,
             unsigned long long page, unsigned long long *addr);

#endif
