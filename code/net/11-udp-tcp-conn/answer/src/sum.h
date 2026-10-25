#ifndef SUM_H
#define SUM_H

/*
 * 16 位反码求和。UDP 和 TCP 的校验和都用它。
 * 进位要加回最低位，不能丢掉。奇数个字节时，最后一字节放在这一字的高 8 位。
 */
unsigned int sum_add_bytes(unsigned int sum, const unsigned char *p, int len);
unsigned int sum_fold(unsigned int sum);
unsigned short sum_finish(unsigned int sum, int zero_as_ffff);

#endif
