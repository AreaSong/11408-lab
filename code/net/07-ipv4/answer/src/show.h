#ifndef SHOW_H
#define SHOW_H

#include "block.h"
#include "cap.h"
#include "frag.h"
#include "hdr.h"

#include <stdio.h>

void show_ip(FILE *fp, const char *title, const IpInfo *ip);
void show_cap(FILE *fp, const CapResult *r, const IpInfo *pkt, uint32_t host);

void show_addr(FILE *fp, uint32_t addr, int prefix);
void show_class(FILE *fp, uint32_t addr);
void show_equal(FILE *fp, Block base, unsigned hosts, const EqualSplit *s);
void show_mask(FILE *fp, Block base, const MaskSplit *s);
void show_vlsm(FILE *fp, Block base, const VlsmSplit *s);
void show_agg(FILE *fp, const Block *in, int n, const AggResult *a, int has_via, uint32_t via);
void show_sibling(FILE *fp, Block known, Block cand, int target, const SiblingResult *r);
void show_remain(FILE *fp, Block net, uint32_t from, uint32_t to, uint32_t router, unsigned used,
                 uint64_t left);

void show_frag(FILE *fp, int total, int header, int mtu, int df, unsigned id, int has_id,
               const FragPlan *plan);

#endif
