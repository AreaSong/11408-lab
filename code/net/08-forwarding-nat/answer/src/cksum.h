#ifndef CKSUM_H
#define CKSUM_H

/* 首部按 16 位一组做反码求和。校验和字段先当 0，结果再按位取反。 */
unsigned ip_checksum(const unsigned char *bytes, int len);

#endif
