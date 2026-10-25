#ifndef SHOW_TOPO_H
#define SHOW_TOPO_H

#include "fwd.h"
#include "fwdlay.h"

void show_domains(const Topo *t, const DomainReport *r);
void show_share(const Topo *t, const DomainReport *r, int a, int b);
void show_fwd(const Topo *t, const FwdTrace *tr);
void show_compare(int n, const DomainReport *hub, const DomainReport *sw, const DomainReport *rt);
void show_delay(int store, long long frame_bytes, const LayTime *t, long long bps);
void show_table(const Device *d);

#endif
