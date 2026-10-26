#ifndef FLOW_H
#define FLOW_H

/* multi：多重中断。single：单重中断。dma：DMA 的三段和三种传送。不是这条命令返回 -1。 */
int flow_command(const char *cmd);

#endif
