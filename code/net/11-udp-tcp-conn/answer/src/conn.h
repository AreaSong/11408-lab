#ifndef CONN_H
#define CONN_H

#define TCP_CLIENT 0
#define TCP_SERVER 1
#define TCP_BOTH 2
#define TCP_STEP_MAX 8

typedef enum {
    ST_CLOSED = 0,
    ST_LISTEN,
    ST_SYN_SENT,
    ST_SYN_RCVD,
    ST_ESTABLISHED,
    ST_FIN_WAIT_1,
    ST_FIN_WAIT_2,
    ST_CLOSE_WAIT,
    ST_CLOSING,
    ST_LAST_ACK,
    ST_TIME_WAIT
} TcpState;

typedef struct {
    TcpState state;
    unsigned int next_seq;
    unsigned int next_ack;
} TcpEnd;

typedef struct {
    TcpEnd end[2];
    int phase;
} TcpLink;

typedef struct {
    int has_seg;
    int sender;
    int syn;
    int ack_flag;
    int fin;
    unsigned int seq;
    int has_ack;
    unsigned int ack;
    unsigned int data_len;
    TcpState client_state;
    TcpState server_state;
    unsigned int client_next;
    unsigned int server_next;
    const char *note;
} TcpStep;

typedef struct {
    int op;
    int side;
    unsigned int arg0;
    unsigned int arg1;
    unsigned int data;
    TcpStep steps[TCP_STEP_MAX];
    int nstep;
} TcpTrace;

const char *tcp_state_name(TcpState state);
void tcp_link_init(TcpLink *link);
const char *tcp_open(TcpLink *link, unsigned int client_isn, unsigned int server_isn,
                     unsigned int data, TcpTrace *tr);
const char *tcp_send(TcpLink *link, int side, unsigned int len, TcpTrace *tr);
const char *tcp_close(TcpLink *link, int who, TcpTrace *tr);

#endif
