#ifndef LIMIT_H
#define LIMIT_H

/*
 * n 位序号，序号空间 N = 2^n，序号在 0～N-1 之间循环。
 * 停等：发送窗口和接收窗口都是 1，1 位序号就够。
 * GBN：接收窗口固定为 1，发送窗口最大 N-1。必须空一个序号，
 * 否则确认全部丢失后，重传的 0 和新一轮的 0 分不清。
 * SR：接收窗口不超过发送窗口，并且两者之和不超过 N。
 * 两边相等时，每个最大是 N/2。
 * 位数算到 16 为止，再大就不是考纲里的窗口题了。
 */

int limit_space(int bits);

int limit_max_gbn(int bits);

int limit_max_sr_equal(int bits);

/* 给定发送窗口，接收窗口的上限。不合法时返回 0。 */
int limit_max_sr_recv(int bits, int ws);

/* 至少几位才能装下这个发送窗口。窗口小于 1 时返回 0。 */
int limit_bits_gbn(int window);

int limit_bits_sr_equal(int window);

/* 题面只说滑动窗口时，取 GBN 和 SR 里更紧的那个位数。 */
int limit_bits_slide(int window);

#endif
