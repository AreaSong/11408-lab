#ifndef DEV_WIRE_H
#define DEV_WIRE_H

/* kind 是 sync、full、half、loose。返回行数，失败返回 -1。result 是这一方式的结论。 */
int dev_wire(const char *kind, char lines[][128], int cap, char *result, int rcap);

#endif
