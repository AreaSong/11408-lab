#include "common/mm_machine.h"
#include "kernel/k_sock.h"
#include "net/net_cong.h"
#include "net/net_dns.h"
#include "net/net_flow.h"
#include "net/net_http.h"
#include "net/net_mail.h"
#include "net/net_sock.h"
#include "net/net_tcp.h"
#include "net/net_udp.h"
#include "test.h"

#include <string.h>

static void expect(int (*fn)(const char *, char *, int), const char *id, const char *text) {
    char buf[192];
    CHECK(fn(id, buf, (int)sizeof buf) == 0);
    CHECK_STR(buf, text);
}

static void test_udp(void) {
    unsigned sum = 0;
    int sport = 0;
    int dport = 0;
    int data = 0;
    int udp = 0;
    int tcp = 0;
    unsigned char byte = 0x41;
    CHECK_EQ((int)net_udp_fold2(0xb9b6u, 0x65c5u), 0xe083);
    CHECK_EQ((int)net_udp_field(0xffffu), 0xffff);
    CHECK(net_udp_cksum(0x0a000001u, 0x0a000002u, 1234, 53, &byte, 1, &sum) == 0);
    CHECK(net_udp_verify(0x0a000001u, 0x0a000002u, 1234, 53, &byte, 1));
    CHECK(net_udp_head("06320045001CE217", &sport, &dport, &data, &sum) == 0);
    CHECK_EQ(sport, 1586);
    CHECK_EQ(dport, 69);
    CHECK_EQ(data, 20);
    CHECK(net_udp_eff(12, &udp, &tcp) == 0);
    CHECK_EQ(udp, 600);
    CHECK_EQ(tcp, 375);
    expect(net_udp_line, "2024-39", "sum=e083 letter=C");
    expect(net_udp_line, "2018-39", "dport=69 name=TFTP data=20 from=client letter=B");
    expect(net_udp_line, "2021-39", "udp=60.0 tcp=37.5 letter=D");
    expect(net_udp_line, "zero", "raw=0000 field=ffff");
}

static void test_tcp(void) {
    unsigned seq = 0;
    unsigned ack = 0;
    int len[] = {300, 400, 500};
    int miss[] = {1, 0, 1};
    int both[] = {1, 1};
    int short_len[] = {300, 500};
    CHECK(net_tcp_hs2(11220u, 11221u, &seq, &ack) == 0);
    CHECK_EQ((int)seq, 11221);
    CHECK_EQ((int)ack, 11221);
    CHECK_EQ((int)net_tcp_hs3(2046u), 2047);
    CHECK_EQ((int)net_tcp_payload(1000u, 5001u), 4000);
    CHECK_EQ((int)net_tcp_cumul(200u, short_len, both, 2), 1000);
    CHECK_EQ((int)net_tcp_cumul(200u, len, miss, 3), 500);
    net_tcp_reply(1913u, 2046u, 100u, &seq, &ack);
    CHECK_EQ((int)seq, 2046);
    CHECK_EQ((int)ack, 2013);
    CHECK_STR(net_tcp_active(3), "TIME_WAIT");
    expect(net_tcp_line, "2011-39", "syn=1 ack=1 seq=11221 ackno=11221 letter=C");
    expect(net_tcp_line, "2019-39", "ackno=2047 letter=D");
    expect(net_tcp_line, "2021-38", "state=TIME_WAIT letter=B");
    expect(net_tcp_line, "2009-38", "ackno=1000 letter=D");
    expect(net_tcp_line, "2011-40", "ackno=500 letter=B");
    expect(net_tcp_line, "2013-39", "seq=2046 ackno=2013 letter=B");
    expect(net_tcp_line, "2020-39", "bytes=4000 letter=C");
    expect(net_tcp_line, "both", "state=CLOSING");
}

static void test_window(void) {
    int cwnd = 0;
    int swnd = 0;
    int rwnd = 0;
    int thresh = 0;
    int seqs[8];
    unsigned seq = 0;
    int rounds = 0;
    int rate = 0;
    int mss = 0;
    CHECK(net_cong_run(16, 0, -1, 0, 1, 4, &cwnd, &swnd, &rwnd) == 0);
    CHECK_EQ(cwnd, 9);
    CHECK(net_cong_run(8, 0, 10, 0, 1, 10, &cwnd, &swnd, &rwnd) == 0);
    CHECK_EQ(cwnd, 12);
    CHECK_EQ(swnd, 10);
    CHECK(net_cong_run(1, 32, -1, 16, 0, 4, &cwnd, &swnd, &rwnd) == 0);
    CHECK_EQ(cwnd, 16);
    CHECK_EQ(swnd, 1);
    CHECK_EQ(rwnd, 1);
    CHECK(net_cong_run(8, 12, -1, 0, 0, 1, &cwnd, &swnd, &rwnd) == 0);
    CHECK_EQ(cwnd, 12);
    CHECK(net_cong_2016(&rwnd, &cwnd, &swnd, &seq, &rounds, &rate) == 0);
    CHECK_EQ(rwnd, 12);
    CHECK_EQ(cwnd, 9);
    CHECK_EQ(swnd, 9);
    CHECK_EQ((int)seq, 20581);
    CHECK_EQ(rounds, 5);
    CHECK_EQ(rate, 16384);
    CHECK_EQ(net_cong_span(1, 32, 1), 5);
    CHECK_EQ(net_cong_span(8, 32, 0), 24);
    CHECK_EQ(net_cong_back(16), 11);
    CHECK(net_cong_fast(16, &thresh, &cwnd) == 0);
    CHECK_EQ(thresh, 8);
    CHECK_EQ(cwnd, 8);
    CHECK(net_cong_trace(16, 4, &thresh, seqs, 8) == 5);
    CHECK_EQ(thresh, 8);
    CHECK_EQ(seqs[0], 1);
    CHECK_EQ(seqs[4], 9);
    CHECK(net_cong_ftp(2101, &mss) == 0);
    CHECK_EQ(mss, 3);
    CHECK(net_cong_ftp(7101, &mss) == 0);
    CHECK_EQ(mss, 5);
}

static void test_cong(void) {
    long rto = 0;
    long karn = 0;
    char buf[192];
    test_window();
    CHECK_EQ(net_flow_left(4000, 2000, 1000), 1000);
    CHECK(net_rto_pair(&rto, &karn) == 0);
    CHECK_EQ(rto, 4750000L);
    CHECK_EQ(karn, 9500000L);
    CHECK(net_rto_line(buf, (int)sizeof buf) == 0);
    CHECK_STR(buf, "rtts=1.5 dev=0.75 rto=4.5 rtts2=1.625 dev2=0.78125 rto2=4.75 karn=9.5");
    expect(net_cong_line, "2009-39", "kb=9 letter=C");
    expect(net_cong_line, "2014-38", "cwnd=12 swnd=10 letter=A");
    expect(net_cong_line, "2015-39", "cwnd=16 swnd=1 letter=A");
    expect(net_cong_line, "2016-41", "rwnd=12 cwnd=9 swnd=9 seq=20581 rtt=5 kbps=163.84");
    expect(net_cong_line, "2017-39", "rtt=5 ms=25 letter=A");
    expect(net_cong_line, "2020-38", "rtt=24 ms=48 letter=D");
    expect(net_cong_line, "2022-38", "rtt=11 letter=C");
    expect(net_cong_line, "fast", "cwnd=8 thresh=8");
    expect(net_cong_line, "cap", "cwnd=12");
    expect(net_flow_line, "2010-39", "left=1000 letter=A");
    expect(net_flow_line, "zero", "seq=301 rwnd=299");
    expect(net_flow_line, "probe", "seq=301 rwnd=0");
}

static void test_dns_http(void) {
    int host = 0;
    int local = 0;
    int total = 0;
    int rtts = 0;
    int cwnd = 0;
    int persist = 1;
    int cookie = 0;
    char text[16];
    static const unsigned char hi[] = {0xE4, 0xBD, 0xA0, 0xE5, 0xA5, 0xBD};
    static const char *req = "GET /index.html HTTP/1.1\r\nConnection: close\r\nCookie: 123\r\n\r\n";
    CHECK(net_dns_count(1, 0, 3, &host, &local, &total) == 0);
    CHECK_EQ(host, 1);
    CHECK_EQ(local, 1);
    CHECK_EQ(total, 8);
    CHECK(net_dns_count(0, 0, 4, &host, &local, &total) == 0);
    CHECK_EQ(local, 4);
    CHECK(net_dns_count(0, 1, 4, &host, &local, &total) == 0);
    CHECK_EQ(local, 0);
    CHECK_EQ(net_http_rtts(7, 0, 1, 0), 16);
    CHECK_EQ(net_http_rtts(7, 1, 1, 0), 9);
    CHECK_EQ(net_http_rtts(7, 2, 1, 0), 3);
    CHECK_EQ(net_http_rtts(5, 1, 1, 1), 6);
    CHECK(net_http_slow(1, 3, &rtts, &cwnd) == 0);
    CHECK_EQ(rtts, 4);
    CHECK_EQ(cwnd, 5);
    CHECK(net_http_req(req, &persist, &cookie) == 0);
    CHECK_EQ(persist, 0);
    CHECK_EQ(cookie, 1);
    CHECK(net_b64_encode(hi, 6, text, (int)sizeof text) == 8);
    CHECK_STR(text, "5L2g5aW9");
    expect(net_dns_line, "2010-40", "host=1 local=1 total=8 letter=A");
    expect(net_dns_line, "2016-40", "min=0 max=4 letter=C");
    expect(net_http_line, "2011-47", "rtt=6");
    expect(net_http_line, "2015-40", "persist=0 cookie=1 wrong=C");
    expect(net_http_line, "2022-40", "rtt=4 ms=40 letter=B");
    expect(net_http_line, "2024-40", "rtt=16 letter=D");
    expect(net_http_line, "modes", "non=16 persist=9 pipe=3");
}

static void test_app(void) {
    char body[800];
    test_dns_http();
    expect(net_mail_line, "2012-40", "a=SMTP b=SMTP c=POP3 letter=D");
    expect(net_mail_line, "2018-40", "body=ASCII letter=D");
    expect(net_mail_line, "2025-40", "items=I,IV letter=A");
    expect(net_mail_line, "b64", "5L2g5aW9");
    expect(net_ftp_line, "2009-40", "tcp=1 ctrl=1 letter=A");
    expect(net_ftp_line, "2017-40", "port20=server wrong=C");
    expect(net_ftp_line, "2023-47", "first=101 ack=18102 cwnd=3,5 ms=60 mbps=2.4");
    expect(net_p2p_line, "1", "cs=30 p2p=30");
    expect(net_p2p_line, "10", "cs=100 p2p=50");
    expect(net_p2p_line, "1000", "cs=10000 p2p=10000/101");
    expect(net_p2p_line, "noupload", "cs=100 p2p=100");
    expect(net_p2p_line, "slow", "cs=7500 p2p=7500");
    expect(net_aexam_line, "2013-40", "items=I,II,III letter=A");
    expect(net_aexam_line, "2014-40", "smtp=0 letter=D");
    expect(net_aexam_line, "2015-33", "tcp=1 letter=D");
    expect(net_aexam_line, "2018-33", "udp=DNS letter=B");
    expect(net_aexam_line, "2019-38", "dup=3 at=t3 letter=C");
    expect(net_aexam_line, "2019-40", "direct=0 letter=B");
    CHECK(net_session_line(body, (int)sizeof body) == 0);
    CHECK(strstr(body, "udp sum=e083\n") == body);
    CHECK(strstr(body, "c=ESTABLISHED s=ESTABLISHED seq=1001 ack=5001\n") != NULL);
    CHECK(strstr(body, "close c=TIME_WAIT s=CLOSED\n") != NULL);
    CHECK(strstr(body, "cwnd=1,2,4,8,9 thresh=8\n") != NULL);
    CHECK(strstr(body, "fast cwnd=8 thresh=8\n") != NULL);
    CHECK(strstr(body, "page rtt=4 ms=40\n") != NULL);
    CHECK(strstr(body, "b64=5L2g5aW9\n") != NULL);
    CHECK(strstr(body, "sock echo=hi\n") != NULL);
}

static void test_sock(void) {
    AppNs *a = net_app_new();
    MmMachine *m = mm_create(4096);
    char buf[8];
    int srv;
    int cli;
    uint32_t sid;
    uint32_t cid;
    uint32_t byte = 0;
    CHECK(a != NULL);
    srv = net_app_socket(a, 2, 80);
    cli = net_app_socket(a, 2, 0);
    CHECK(srv >= 0);
    CHECK(cli >= 0);
    CHECK(net_app_connect(a, cli, 0x7f000001u, 80) == 0);
    CHECK(net_app_send(a, cli, "ok", 2) == 2);
    CHECK(net_app_recv(a, srv, buf, 8) == 2);
    CHECK(buf[0] == 'o' && buf[1] == 'k');
    net_app_free(a);
    CHECK(m != NULL);
    CHECK(k_app_on(m) == 0);
    CHECK(k_sock_syscall(m, K_SYS_SOCKET, 1, 9000, 0) == 0);
    sid = mm_reg(m, 0);
    CHECK(k_sock_syscall(m, K_SYS_SOCKET, 1, 0, 0) == 0);
    cid = mm_reg(m, 0);
    CHECK(k_sock_syscall(m, K_SYS_CONNECT, cid, 0x7f000001u, 9000) == 0);
    CHECK_EQ((int)mm_reg(m, 0), 0);
    CHECK(mm_write(m, 0x100u, 1, 'h') == 0);
    CHECK(mm_write(m, 0x101u, 1, 'i') == 0);
    CHECK(k_sock_syscall(m, K_SYS_SENDTO, cid, 0x100u, 2) == 0);
    CHECK_EQ((int)mm_reg(m, 0), 2);
    CHECK(k_sock_syscall(m, K_SYS_RECVFROM, sid, 0x200u, 8) == 0);
    CHECK_EQ((int)mm_reg(m, 0), 2);
    CHECK(mm_read(m, 0x200u, 1, &byte) == 0);
    CHECK_EQ((int)byte, 'h');
    CHECK(mm_read(m, 0x201u, 1, &byte) == 0);
    CHECK_EQ((int)byte, 'i');
    mm_destroy(m);
}

int main(void) {
    test_udp();
    test_tcp();
    test_cong();
    test_app();
    test_sock();
    return TEST_SUMMARY();
}
