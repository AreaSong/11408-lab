#ifndef EAT_H
#define EAT_H

int eat_avg(unsigned long long hit, unsigned long long den, unsigned long long tlb,
            unsigned long long mem, int walks, unsigned long long *num, unsigned long long *den_out);
int eat_rate(unsigned long long ok, unsigned long long den, unsigned long long normal,
             unsigned long long fault, unsigned long long *num, unsigned long long *den_out);
int eat_tlb_hit(unsigned long long tlb, unsigned long long mem, unsigned long long *ns);
int eat_pt_hit(unsigned long long tlb, unsigned long long mem, int walks, unsigned long long *ns);
int eat_fault(unsigned long long tlb, unsigned long long mem, int walks, unsigned long long fault,
              unsigned long long *ns);

#endif
