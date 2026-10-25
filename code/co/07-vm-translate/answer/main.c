/*
 * 地址翻译的入口。TLB、页表、Cache 的过程在 src/ 里打印，这里只拆命令。
 */
#include "extra.h"
#include "fmt.h"
#include "geom.h"
#include "map.h"
#include "prompt.h"
#include "series.h"
#include "trans.h"

#include <stdio.h>
#include <string.h>

static int read_word(char *buf) {
    return scanf("%63s", buf) == 1;
}

static int read_u64(uint64_t *out) {
    char buf[64];
    if (!read_word(buf) || !fmt_parse(buf, out)) {
        return 1;
    }
    return 0;
}

static int read_i(int *out) {
    uint64_t value;
    if (read_u64(&value) != 0 || value > 1000000000ull) {
        return 1;
    }
    *out = (int)value;
    return 0;
}

static int run_page(void) {
    int va_bits = 0;
    int pa_bits = 0;
    int page_bytes = 0;
    PageGeom page;
    prompt("请输入「虚拟地址位数  物理地址位数  页字节」，例如 32 28 4096：");
    if (read_i(&va_bits) || read_i(&pa_bits) || read_i(&page_bytes) || page_geom(&page, va_bits, pa_bits, page_bytes) != 0) {
        fprintf(stderr, "page 需要：虚拟地址位数、物理地址位数、页字节\n");
        return 1;
    }
    return show_page(stdout, &page);
}

static int run_cut(void) {
    int va_bits = 0;
    int pa_bits = 0;
    int page_bytes = 0;
    uint64_t va = 0;
    PageGeom page;
    prompt("请输入「虚拟地址位数  物理地址位数  页字节  虚拟地址」，例如 32 24 4096 00082840H：");
    if (read_i(&va_bits) || read_i(&pa_bits) || read_i(&page_bytes) || read_u64(&va) ||
        page_geom(&page, va_bits, pa_bits, page_bytes) != 0) {
        fprintf(stderr, "cut 需要：虚拟地址位数、物理地址位数、页字节、虚拟地址\n");
        return 1;
    }
    return show_cut(stdout, &page, va);
}

static int run_tlb(void) {
    int va_bits = 0;
    int pa_bits = 0;
    int page_bytes = 0;
    int lines = 0;
    int ways = 0;
    PageGeom page;
    TlbGeom tlb;
    prompt("请输入「虚拟地址位数  物理地址位数  页字节  TLB项数  路数」，例如 32 30 1024 32 4：");
    if (read_i(&va_bits) || read_i(&pa_bits) || read_i(&page_bytes) || read_i(&lines) || read_i(&ways) ||
        page_geom(&page, va_bits, pa_bits, page_bytes) != 0 || tlb_geom(&tlb, page.vpn_bits, lines, ways) != 0) {
        fprintf(stderr, "tlb 需要：虚拟地址位数、物理地址位数、页字节、TLB 项数、路数\n");
        return 1;
    }
    return show_tlb(stdout, &page, &tlb);
}

static int run_cfield(void) {
    int pa_bits = 0;
    int lines = 0;
    int block = 0;
    int ways = 0;
    Fields fields;
    prompt("请输入「物理地址位数  Cache行数  块字节  路数」，例如 20 8 32 1：");
    if (read_i(&pa_bits) || read_i(&lines) || read_i(&block) || read_i(&ways) ||
        cache_fields(&fields, pa_bits, lines, block, ways) != 0) {
        fprintf(stderr, "cfield 需要：物理地址位数、Cache 行数、块字节、路数\n");
        return 1;
    }
    return show_cfield(stdout, &fields);
}

static int run_band(void) {
    int va_bits = 0;
    int pa_bits = 0;
    int page_bytes = 0;
    int tlb_lines = 0;
    int tlb_ways = 0;
    int cache_bytes = 0;
    int block = 0;
    int ways = 0;
    PageGeom page;
    TlbGeom tlb;
    Fields fields;
    prompt("请输入「虚拟地址位数  物理地址位数  页字节  TLB项数  TLB路数  Cache数据区字节  块字节  Cache路数」：");
    if (read_i(&va_bits) || read_i(&pa_bits) || read_i(&page_bytes) || read_i(&tlb_lines) || read_i(&tlb_ways) ||
        read_i(&cache_bytes) || read_i(&block) || read_i(&ways) || page_geom(&page, va_bits, pa_bits, page_bytes) != 0 ||
        tlb_geom(&tlb, page.vpn_bits, tlb_lines, tlb_ways) != 0 ||
        cache_from_bytes(&fields, pa_bits, cache_bytes, block, ways) != 0) {
        fprintf(stderr, "band 的参数没读全\n");
        return 1;
    }
    return show_band(stdout, &page, &tlb, &fields);
}

static int run_blk(void) {
    int pa_bits = 0;
    int cache_bytes = 0;
    int block = 0;
    int ways = 0;
    uint64_t block_no = 0;
    Fields fields;
    prompt("请输入「物理地址位数  Cache数据区字节  块字节  路数  主存块号」，例如 24 65536 64 2 4099：");
    if (read_i(&pa_bits) || read_i(&cache_bytes) || read_i(&block) || read_i(&ways) || read_u64(&block_no) ||
        cache_from_bytes(&fields, pa_bits, cache_bytes, block, ways) != 0) {
        fprintf(stderr, "blk 的参数没读全\n");
        return 1;
    }
    return show_blk(stdout, &fields, block_no);
}

static int run_cset(void) {
    char mode[32];
    int pa_bits = 0;
    int page_bytes = 0;
    int lines = 0;
    int block = 0;
    int ways = 0;
    uint64_t addr = 0;
    PageGeom page;
    Fields fields;
    int low_only;
    prompt("请输入「low 或 pa  物理地址位数  页字节  Cache行数  块字节  路数  地址」：");
    if (!read_word(mode) || (strcmp(mode, "low") != 0 && strcmp(mode, "pa") != 0) || read_i(&pa_bits) ||
        read_i(&page_bytes) || read_i(&lines) || read_i(&block) || read_i(&ways) || read_u64(&addr) ||
        page_geom(&page, pa_bits, pa_bits, page_bytes) != 0 || cache_fields(&fields, pa_bits, lines, block, ways) != 0) {
        fprintf(stderr, "cset 需要：low 或 pa、物理地址位数、页字节、Cache 行数、块字节、路数、地址\n");
        return 1;
    }
    low_only = strcmp(mode, "low") == 0;
    return show_cset(stdout, &page, &fields, addr, low_only);
}

static int run_series(void) {
    int va_bits = 0;
    int page_bytes = 0;
    int lines = 0;
    int ways = 0;
    int n = 0;
    int i;
    uint64_t vpns[64];
    prompt("请输入「虚拟地址位数  页字节  TLB项数  路数  访问个数」，下一行起每个虚页号一个数：");
    if (read_i(&va_bits) || read_i(&page_bytes) || read_i(&lines) || read_i(&ways) || read_i(&n) || n < 1 || n > 64) {
        fprintf(stderr, "series 的访问个数要在 1 到 64 之间\n");
        return 1;
    }
    for (i = 0; i < n; i++) {
        if (read_u64(&vpns[i]) != 0) {
            fprintf(stderr, "第 %d 个虚页号没读到\n", i + 1);
            return 1;
        }
    }
    return show_series(stdout, va_bits, page_bytes, lines, ways, vpns, n);
}

static int run_pt(void) {
    int va_bits = 0;
    int pa_bits = 0;
    int page_bytes = 0;
    int n = 0;
    int i;
    uint64_t va = 0;
    PageGeom page;
    PteItem ptes[TRANS_MAX];
    prompt("请输入「虚拟地址位数  物理地址位数  页字节  页表项个数」，接着每行「虚页号  存在位  页框号」，最后是虚拟地址：");
    if (read_i(&va_bits) || read_i(&pa_bits) || read_i(&page_bytes) || read_i(&n) || n < 1 || n > TRANS_MAX ||
        page_geom(&page, va_bits, pa_bits, page_bytes) != 0) {
        fprintf(stderr, "pt 的页表项个数要在 1 到 %d 之间\n", TRANS_MAX);
        return 1;
    }
    for (i = 0; i < n; i++) {
        int present = 0;
        if (read_u64(&ptes[i].vpn) || read_i(&present) || read_u64(&ptes[i].frame) || (present != 0 && present != 1)) {
            fprintf(stderr, "页表项要写成：虚页号 存在位 页框号\n");
            return 1;
        }
        ptes[i].present = present;
    }
    if (read_u64(&va) != 0) {
        fprintf(stderr, "pt 最后要给虚拟地址\n");
        return 1;
    }
    return show_pt(stdout, &page, ptes, n, va);
}

static int run_pte(void) {
    int va_bits = 0;
    int page_bytes = 0;
    int pte_bytes = 0;
    uint64_t pt_va = 0;
    uint64_t pt_pa = 0;
    uint64_t va = 0;
    uint64_t final_pa = 0;
    prompt("请输入「地址位数  页字节  页表项字节  页表虚拟始址  页表物理始址  虚拟地址  缺页处理后的物理地址」：");
    if (read_i(&va_bits) || read_i(&page_bytes) || read_i(&pte_bytes) || read_u64(&pt_va) || read_u64(&pt_pa) ||
        read_u64(&va) || read_u64(&final_pa)) {
        fprintf(stderr, "pte 的参数没读全\n");
        return 1;
    }
    return show_pte(stdout, va_bits, page_bytes, pte_bytes, pt_va, pt_pa, va, final_pa);
}

static int run_seg(void) {
    uint64_t base = 0;
    uint64_t limit = 0;
    uint64_t offset = 0;
    prompt("请输入「段基址  段长  段内偏移」，例如 8000H 100H 20H：");
    if (read_u64(&base) || read_u64(&limit) || read_u64(&offset)) {
        fprintf(stderr, "seg 需要：段基址、段长、段内偏移\n");
        return 1;
    }
    return show_seg(stdout, base, limit, offset);
}

static int run_segpage(void) {
    int page_bytes = 0;
    int present = 0;
    uint64_t n_pages = 0;
    uint64_t page_no = 0;
    uint64_t offset = 0;
    uint64_t frame = 0;
    prompt("请输入「页字节  段内页数  页号  页内偏移  存在位  页框号」：");
    if (read_i(&page_bytes) || read_u64(&n_pages) || read_u64(&page_no) || read_u64(&offset) || read_i(&present) ||
        read_u64(&frame) || (present != 0 && present != 1)) {
        fprintf(stderr, "segpage 需要：页字节、段内页数、页号、页内偏移、存在位、页框号\n");
        return 1;
    }
    return show_segpage(stdout, page_bytes, n_pages, page_no, offset, present, frame);
}

static int put_tlb(TlbSlot *slots, int n_sets, int n_ways, int set, int way, int valid, uint64_t tag, uint64_t frame) {
    int index;
    if (set < 0 || set >= n_sets || way < 0 || way >= n_ways || (valid != 0 && valid != 1)) {
        fprintf(stderr, "TLB 的组号、路号或有效位不合法\n");
        return 1;
    }
    index = set * n_ways + way;
    if (slots[index].given) {
        fprintf(stderr, "TLB 组 %d 路 %d 写了两次\n", set, way);
        return 1;
    }
    slots[index].given = 1;
    slots[index].valid = valid;
    slots[index].tag = tag;
    slots[index].frame = frame;
    return 0;
}

static int read_tlb(TlbSlot *slots, int n_sets, int n_ways, int n) {
    int i;
    for (i = 0; i < n; i++) {
        int set = 0;
        int way = 0;
        int valid = 0;
        uint64_t tag = 0;
        uint64_t frame = 0;
        if (read_i(&set) || read_i(&way) || read_i(&valid) || read_u64(&tag) || read_u64(&frame) ||
            put_tlb(slots, n_sets, n_ways, set, way, valid, tag, frame) != 0) {
            fprintf(stderr, "TLB 项要写成：组号 路号 有效位 标记 页框号\n");
            return 1;
        }
    }
    return 0;
}

static int put_line(LineSlot *lines, int n_sets, int n_ways, int set, int way, int valid, uint64_t tag) {
    int index;
    if (set < 0 || set >= n_sets || way < 0 || way >= n_ways || (valid != 0 && valid != 1)) {
        fprintf(stderr, "Cache 的组号、路号或有效位不合法\n");
        return 1;
    }
    index = set * n_ways + way;
    if (lines[index].given) {
        fprintf(stderr, "Cache 组 %d 路 %d 写了两次\n", set, way);
        return 1;
    }
    lines[index].given = 1;
    lines[index].valid = valid;
    lines[index].tag = tag;
    return 0;
}

static int read_lines(LineSlot *lines, int n_sets, int n_ways, int n) {
    int i;
    for (i = 0; i < n; i++) {
        int set = 0;
        int way = 0;
        int valid = 0;
        uint64_t tag = 0;
        if (read_i(&set) || read_i(&way) || read_i(&valid) || read_u64(&tag) ||
            put_line(lines, n_sets, n_ways, set, way, valid, tag) != 0) {
            fprintf(stderr, "Cache 行要写成：组号 路号 有效位 标记\n");
            return 1;
        }
    }
    return 0;
}

static int read_ptes(PteItem *ptes, int n) {
    int i;
    for (i = 0; i < n; i++) {
        int present = 0;
        if (read_u64(&ptes[i].vpn) || read_i(&present) || read_u64(&ptes[i].frame) || (present != 0 && present != 1)) {
            fprintf(stderr, "页表项要写成：虚页号 存在位 页框号\n");
            return 1;
        }
        ptes[i].present = present;
    }
    return 0;
}

static int load_cache(Fields *fields, LineSlot *lines, int pa_bits) {
    char word[32];
    int n_lines = 0;
    int block = 0;
    int ways = 0;
    int n_snap = 0;
    if (!read_word(word)) {
        fprintf(stderr, "walk 接着要写 cache 或 nocache\n");
        return 1;
    }
    if (strcmp(word, "nocache") == 0) {
        return 0;
    }
    if (strcmp(word, "cache") != 0 || read_i(&n_lines) || read_i(&block) || read_i(&ways) ||
        cache_fields(fields, pa_bits, n_lines, block, ways) != 0) {
        fprintf(stderr, "cache 后面是：行数 块字节 路数\n");
        return 1;
    }
    if (fields->n_lines > TRANS_MAX) {
        fprintf(stderr, "Cache 快照最多 %d 行\n", TRANS_MAX);
        return 1;
    }
    if (read_i(&n_snap) || n_snap < 0 || n_snap > fields->n_lines ||
        read_lines(lines, fields->n_sets, fields->n_ways, n_snap) != 0) {
        return 1;
    }
    return 2;
}

static int attach_tlb(TransIn *in, TlbGeom *tlb, TlbSlot *slots, const PageGeom *page, int tlb_lines, int tlb_ways) {
    int n_tlb = 0;
    if (tlb_lines == 0 && tlb_ways == 0) {
        if (read_i(&n_tlb) || n_tlb != 0) {
            fprintf(stderr, "没有 TLB 时，给出的项数要写 0\n");
            return 1;
        }
        return 0;
    }
    if (tlb_geom(tlb, page->vpn_bits, tlb_lines, tlb_ways) != 0) {
        return 1;
    }
    if (tlb->n_lines > TRANS_MAX) {
        fprintf(stderr, "TLB 快照最多 %d 项\n", TRANS_MAX);
        return 1;
    }
    if (read_i(&n_tlb) || n_tlb < 0 || n_tlb > tlb->n_lines) {
        fprintf(stderr, "TLB 项个数要在 0 到 %d 之间\n", tlb->n_lines);
        return 1;
    }
    if (read_tlb(slots, tlb->n_sets, tlb->n_ways, n_tlb) != 0) {
        return 1;
    }
    in->tlb = tlb;
    in->slots = slots;
    return 0;
}

static int run_walk(void) {
    int va_bits = 0;
    int pa_bits = 0;
    int page_bytes = 0;
    int tlb_lines = 0;
    int tlb_ways = 0;
    int n_pte = 0;
    int cache_flag;
    uint64_t va = 0;
    PageGeom page;
    TlbGeom tlb;
    TlbSlot slots[TRANS_MAX];
    PteItem ptes[TRANS_MAX];
    LineSlot lines[TRANS_MAX];
    Fields fields;
    TransIn in;
    prompt("请输入「虚拟地址位数  物理地址位数  页字节  TLB项数  TLB路数」。没有 TLB 时项数和路数都填 0。\n"
           "下一行是给出的 TLB 项个数，每项「组号 路号 有效位 标记 页框号」。\n"
           "然后是页表项个数，每项「虚页号 存在位 页框号」。\n"
           "然后 cache 行数 块字节 路数，或 nocache；cache 再跟快照条数和「组号 路号 有效位 标记」。\n"
           "最后一行是虚拟地址：");
    memset(slots, 0, sizeof slots);
    memset(lines, 0, sizeof lines);
    memset(&in, 0, sizeof in);
    if (read_i(&va_bits) || read_i(&pa_bits) || read_i(&page_bytes) || read_i(&tlb_lines) || read_i(&tlb_ways)) {
        fprintf(stderr, "walk 开头五个数没读全\n");
        return 1;
    }
    if (page_geom(&page, va_bits, pa_bits, page_bytes) != 0 ||
        attach_tlb(&in, &tlb, slots, &page, tlb_lines, tlb_ways) != 0) {
        return 1;
    }
    if (read_i(&n_pte) || n_pte < 0 || n_pte > TRANS_MAX || read_ptes(ptes, n_pte) != 0) {
        return 1;
    }
    cache_flag = load_cache(&fields, lines, pa_bits);
    if (cache_flag == 1 || read_u64(&va) != 0) {
        return 1;
    }
    in.page = &page;
    in.ptes = ptes;
    in.n_pte = n_pte;
    in.va = va;
    if (cache_flag == 2) {
        in.cache = &fields;
        in.lines = lines;
    }
    return show_access(stdout, &in);
}

int main(void) {
    char cmd[32];
    for (;;) {
        int rc;
        prompt("请输入命令（page / cut / pt / tlb / series / band / blk / cfield / cset / walk / pte / seg / "
               "segpage，Ctrl+D 结束）：");
        if (!read_word(cmd)) {
            break;
        }
        if (strcmp(cmd, "page") == 0) {
            rc = run_page();
        } else if (strcmp(cmd, "cut") == 0) {
            rc = run_cut();
        } else if (strcmp(cmd, "pt") == 0) {
            rc = run_pt();
        } else if (strcmp(cmd, "tlb") == 0) {
            rc = run_tlb();
        } else if (strcmp(cmd, "series") == 0) {
            rc = run_series();
        } else if (strcmp(cmd, "band") == 0) {
            rc = run_band();
        } else if (strcmp(cmd, "blk") == 0) {
            rc = run_blk();
        } else if (strcmp(cmd, "cfield") == 0) {
            rc = run_cfield();
        } else if (strcmp(cmd, "cset") == 0) {
            rc = run_cset();
        } else if (strcmp(cmd, "walk") == 0) {
            rc = run_walk();
        } else if (strcmp(cmd, "pte") == 0) {
            rc = run_pte();
        } else if (strcmp(cmd, "seg") == 0) {
            rc = run_seg();
        } else if (strcmp(cmd, "segpage") == 0) {
            rc = run_segpage();
        } else {
            fprintf(stderr, "未知命令：%s\n", cmd);
            return 1;
        }
        if (rc != 0) {
            return rc;
        }
    }
    return 0;
}
