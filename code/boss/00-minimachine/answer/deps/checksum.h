#ifndef CHECKSUM_H
#define CHECKSUM_H

/* 16 位反码求和用的累加。最后一字节单独成字时，它放在高 8 位。 */
unsigned int checksum_add(unsigned int sum, const unsigned char *p, int len);
unsigned int checksum_fold(unsigned int sum);

/* 首部校验和：计算前把校验和字段当成 0。不加伪首部。 */
unsigned short ip_header_checksum(const unsigned char *hdr, int hdr_len);

/*
 * TCP 校验和要加 12 字节伪首部。伪首部不出现在线上。
 * 算出 0 时写成 0xffff：TCP 没有“不算校验和”的编码，0 留不出这个意思。
 */
unsigned short tcp_checksum(const unsigned char src[4], const unsigned char dst[4],
                            const unsigned char *segment, int seg_len);

#endif
