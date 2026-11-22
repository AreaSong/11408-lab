#ifndef ALOHA_H
#define ALOHA_H

/*
 * 考纲：随机访问 ALOHA。G 是一个帧时内全网平均尝试次数，含重传。
 * 纯 ALOHA 易受攻击时间是 2 个帧时，S = G*e^(-2G)，最大在 G = 0.5，Smax = 1/(2e)。
 * 时隙 ALOHA 只在时隙起点发，S = G*e^(-G)，最大在 G = 1，Smax = 1/e。两种都不听信道。
 */

/* text 写成四位小数，和 printf 的 %.4f 一致。at_max 在最大点为 1。 */
int aloha_s(int slotted, double g, char text[16], int *at_max);

/* 整数不带小数点，其余最多四位并去掉末尾 0。 */
void aloha_num(double x, char *buf, int cap);

/* 百分数是 100/(2e)、100/e 保留一位，王道写成 18.4% 和 36.8%。 */
void aloha_peak(char pure_pct[8], char slot_pct[8], char pure_s[16], char slot_s[16]);

#endif
