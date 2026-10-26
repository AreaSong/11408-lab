#ifndef HASH_H
#define HASH_H

/*
 * 除留余数法 H(key) = (a * key) % p。
 * 探测在表长 m 上取模；查找失败的 ASL 只对 0..p-1 这些散列地址平均，分母是 p 不是 m。
 * 平方探测默认先 +k^2 再 -k^2（王道）。qpos 只加 k^2，用来对 2024 年的题面。
 */

#define HASH_M_MAX 64
#define HASH_N_MAX 64
#define HASH_KEY_MAX 1000000
#define HASH_A_MAX 1000000
#define HASH_THEORY_MAX 10000

enum {
    HASH_OK = 0,
    HASH_DUP = 1,
    HASH_FULL = 2,
    HASH_MISS = 3,
    HASH_KIND = 4
};

enum {
    KIND_NONE = 0,
    KIND_LINEAR = 1,
    KIND_QUAD = 2,
    KIND_QPOS = 3,
    KIND_DOUBLE = 4,
    KIND_CHAIN = 5
};

enum {
    CELL_EMPTY = 0,
    CELL_FULL = 1,
    CELL_TOMB = 2
};

typedef struct HashNode {
    int key;
    struct HashNode *next;
} HashNode;

typedef struct {
    int kind;
    int m;
    int p;
    int a;
    int b; /* H(key) = (a * key + b) % p，2023 年的 +4 用这个加数 */
    int q;
    int n;
    int *key;
    int *state;
    HashNode **head;
} HashTab;

/* cell_* 是走到这一格时的快照。插入会先记再写，所以空位在路径里仍显示为 -。 */
typedef struct {
    int rc;
    int h;
    int cmp;
    int put;
    int npath;
    int pos[HASH_M_MAX];
    int cell_state[HASH_M_MAX];
    int cell_key[HASH_M_MAX];
} OpenWalk;

typedef struct {
    int rc;
    int h;
    int cmp;
    int npath;
    int key[HASH_M_MAX];
} ChainWalk;

typedef struct {
    int has_succ;
    long long succ_num;
    long long succ_den;
    int depend;
    long long fail_num;
    long long fail_den;
} HashAsl;

typedef struct {
    int succ_none;
    int succ_exact;
    int fail_exact;
    long long succ_num;
    long long succ_den;
    long long fail_num;
    long long fail_den;
    double succ_dec;
    double fail_dec;
} HashFormula;

const char *hash_kind_name(int kind);
int hash_kind_from(const char *name);

HashTab *hash_create(int kind, int m, int p, int a, int q);
void hash_free(HashTab *t);
int hash_addr(const HashTab *t, int key);
int hash_step(const HashTab *t, int key);
int hash_probe(const HashTab *t, int h0, int step, int *pos);
int hash_insert(HashTab *t, int key);

int open_insert(HashTab *t, int key, OpenWalk *w);
int open_find(const HashTab *t, int key, OpenWalk *w);
int open_delete(HashTab *t, int key, int wipe, OpenWalk *w);

int chain_insert(HashTab *t, int key, ChainWalk *w);
int chain_find(const HashTab *t, int key, ChainWalk *w);
int chain_delete(HashTab *t, int key, ChainWalk *w);

void hash_asl(const HashTab *t, int count_null, HashAsl *out);
int hash_formula(int kind, long long n, long long m, HashFormula *out);
long long hash_gcd(long long a, long long b);
void hash_print_frac(const char *name, long long num, long long den);

int hash_run(const HashTab *t);

void show_open_ins(int key, const OpenWalk *w);
void show_open_query(const char *cmd, int key, const OpenWalk *w);
void show_open_del(const char *cmd, int key, const OpenWalk *w, int wipe);
void show_chain_ins(int key, const ChainWalk *w);
void show_chain_query(const char *cmd, int key, const ChainWalk *w);
void show_chain_del(int key, const ChainWalk *w);
void show_summary(const HashTab *t);
void show_asl(const HashTab *t, int count_null);
void show_formula(int kind, long long n, long long m);

void fact_clash(void);
void fact_improve(void);
void fact_grow(int kind, int p, int n, int m);

#endif
