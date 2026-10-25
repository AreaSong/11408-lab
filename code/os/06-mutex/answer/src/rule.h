#ifndef RULE_H
#define RULE_H

/* 软件方法和硬件指令在步进器里；整型、记录型信号量只给四条准则，不步进。 */
enum {
    ALGO_SINGLE = 0,
    ALGO_CHECK_FIRST,
    ALGO_CHECK_AFTER,
    ALGO_PETERSON,
    ALGO_TSL,
    ALGO_SWAP,
    ALGO_INTEGER,
    ALGO_RECORD,
    ALGO_COUNT
};

int rule_id(const char *name);
const char *rule_name(int id);
const char *rule_line(int id);
int rule_count(void);
const char *rule_at(int index);
int rule_can_step(int id);

#endif
