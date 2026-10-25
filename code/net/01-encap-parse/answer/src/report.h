#ifndef REPORT_H
#define REPORT_H

#include <stdio.h>

#include "packet.h"

void report_encap(FILE *fp, const Packet *pkt);
void report_parse(FILE *fp, const Packet *pkt);

#endif
