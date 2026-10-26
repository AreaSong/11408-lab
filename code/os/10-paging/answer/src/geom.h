#ifndef GEOM_H
#define GEOM_H

/* 页号位数最多 63，每级至少 1 位，级数不会超过 63。 */
#define GEOM_LEVELS 64

typedef struct {
    int va_bits;
    int offset_bits;
    int page_bits;
    int level_bits;
    int levels;
    int top_bits;
    int fits;
    int table_overflow;
    int terms_overflow;
    int nterms;
    unsigned long long page_bytes;
    unsigned long long pte_bytes;
    unsigned long long pages;
    unsigned long long table_bytes;
    unsigned long long per_page;
    unsigned long long top_entries;
    unsigned long long top_bytes;
    unsigned long long tables_pages;
    unsigned long long terms[GEOM_LEVELS];
} Geom;

int geom_bits(int va_bits, unsigned long long page, unsigned long long pte, Geom *out);
int geom_pages(unsigned long long logical_pages, unsigned long long page,
               unsigned long long pte, Geom *out);

#endif
