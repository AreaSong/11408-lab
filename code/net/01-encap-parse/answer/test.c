#include <string.h>

#include <arpa/inet.h>

#include "bytes.h"
#include "packet.h"
#include "pcapio.h"
#include "test.h"

static const unsigned char k2011[80] = {
    0x00, 0x21, 0x27, 0x21, 0x51, 0xee, 0x00, 0x15, 0xc5, 0xc1, 0x5e, 0x28, 0x08, 0x00, 0x45, 0x00,
    0x01, 0xef, 0x11, 0x3b, 0x40, 0x00, 0x80, 0x06, 0xba, 0x9d, 0x0a, 0x02, 0x80, 0x64, 0x40, 0xaa,
    0x62, 0x20, 0x04, 0xff, 0x00, 0x50, 0xe0, 0xe2, 0x00, 0xfa, 0x7b, 0xf9, 0xf8, 0x05, 0x50, 0x18,
    0xfa, 0xf0, 0x1a, 0xc4, 0x00, 0x00, 0x47, 0x45, 0x54, 0x20, 0x2f, 0x72, 0x66, 0x63, 0x2e, 0x68,
    0x74, 0x6d, 0x6c, 0x20, 0x48, 0x54, 0x54, 0x50, 0x2f, 0x31, 0x2e, 0x31, 0x0d, 0x0a, 0x41, 0x63,
};

static void test_text(void) {
    unsigned char mac[6];
    unsigned char ip[4];
    unsigned char flags = 0;
    unsigned char raw[8];
    unsigned int v = 0;
    int n = 0;
    char err[64];
    char mac_text[24];
    char ip_text[20];

    CHECK(parse_mac("00-21-27-21-51-ee", mac));
    CHECK_EQ(mac[0], 0x00);
    CHECK_EQ(mac[5], 0xee);
    CHECK(!parse_mac("00:11:22:33:44", mac));
    CHECK(parse_ipv4("64.170.98.32", ip));
    CHECK_EQ(ip[0], 64);
    CHECK_EQ(ip[3], 32);
    CHECK(!parse_ipv4("10.2.128.256", ip));
    CHECK(parse_u32("0x113b", &v));
    CHECK_EQ(v, 0x113b);
    CHECK(parse_u32("128", &v));
    CHECK_EQ(v, 128);
    CHECK(parse_tcp_flags("psh,ack", &flags));
    CHECK_EQ(flags, 0x18);
    CHECK(unescape("Hi\\r\\n", raw, 8, &n, err, (int)sizeof err));
    CHECK_EQ(n, 4);
    CHECK_EQ(raw[2], '\r');
    CHECK_EQ(raw[3], '\n');
    format_mac(mac, mac_text, (int)sizeof mac_text);
    CHECK_STR(mac_text, "00-21-27-21-51-ee");
    format_ipv4(ip, ip_text, (int)sizeof ip_text);
    CHECK_STR(ip_text, "64.170.98.32");
}

static void test_byte_order(void) {
    unsigned char b[4];
    unsigned short net16 = htons(0x0800);
    unsigned int net32 = htonl(0x0a028064u);

    memcpy(b, &net16, 2);
    CHECK_EQ(b[0], 0x08);
    CHECK_EQ(b[1], 0x00);
    CHECK_EQ(read_be16(b), 0x0800);
    memcpy(b, &net32, 4);
    CHECK_EQ(read_be32(b), 0x0a028064u);
}

static void test_2011(void) {
    Packet pkt;
    char err[160];
    char dst_mac[24];
    char src_ip[20];
    char dst_ip[20];

    CHECK(packet_parse(k2011, 80, &pkt, err, (int)sizeof err));
    CHECK(pkt.has_ip);
    CHECK(pkt.has_tcp);
    CHECK_EQ(pkt.ip.total_len, 495);
    CHECK_EQ(pkt.ip.id, 0x113b);
    CHECK_EQ(pkt.ip.ttl, 128);
    CHECK_EQ(pkt.ip.protocol, 6);
    CHECK_EQ(pkt.ip.checksum, 0xba9d);
    CHECK_EQ(pkt.ip_checksum_ok, 1);
    CHECK_EQ(pkt.ip.flags_frag, 0x4000);
    CHECK_EQ(pkt.tcp.src_port, 1279);
    CHECK_EQ(pkt.tcp.dst_port, 80);
    CHECK_EQ(pkt.tcp.seq, 0xe0e200fau);
    CHECK_EQ(pkt.tcp.ack, 0x7bf9f805u);
    CHECK_EQ(pkt.tcp.flags, 0x18);
    CHECK_EQ(pkt.tcp.window, 64240);
    CHECK_EQ(pkt.payload_total, 455);
    CHECK_EQ(pkt.payload_len, 26);
    CHECK_EQ(pkt.implied_no_fcs, 509);
    CHECK_EQ(pkt.pad_len, 0);
    CHECK_EQ(pkt.tcp_checksum_ok, -1);
    CHECK(memcmp(pkt.payload, "GET /rfc.html HTTP/1.1\r\nAc", 26) == 0);
    format_mac(pkt.eth.dst, dst_mac, (int)sizeof dst_mac);
    format_ipv4(pkt.ip.src, src_ip, (int)sizeof src_ip);
    format_ipv4(pkt.ip.dst, dst_ip, (int)sizeof dst_ip);
    CHECK_STR(dst_mac, "00-21-27-21-51-ee");
    CHECK_STR(src_ip, "10.2.128.100");
    CHECK_STR(dst_ip, "64.170.98.32");
}

static int fill_http(Packet *pkt, const char *text) {
    char err[64];
    int n = 0;

    memset(pkt, 0, sizeof *pkt);
    if (!parse_mac("00-11-22-33-44-55", pkt->eth.dst) || !parse_mac("00-aa-bb-cc-dd-ee", pkt->eth.src) ||
        !parse_ipv4("192.168.1.2", pkt->ip.src) || !parse_ipv4("192.168.1.1", pkt->ip.dst) ||
        !unescape(text, pkt->payload, PAYLOAD_MAX, &n, err, (int)sizeof err) ||
        !parse_tcp_flags("psh,ack", &pkt->tcp.flags)) {
        return 0;
    }
    pkt->payload_len = n;
    pkt->ip.ttl = 64;
    pkt->ip.id = 1;
    pkt->tcp.src_port = 50000;
    pkt->tcp.dst_port = 80;
    pkt->tcp.seq = 1;
    pkt->tcp.ack = 0;
    pkt->tcp.window = 8192;
    return 1;
}

static void test_roundtrip(void) {
    Packet pkt;
    Packet back;
    unsigned char with_fcs[80];
    char err[160];

    CHECK(fill_http(&pkt, "GET / HTTP/1.1\\r\\n"));
    CHECK(packet_build(&pkt, err, (int)sizeof err));
    CHECK_EQ(pkt.payload_len, 16);
    CHECK_EQ(pkt.ip.total_len, 56);
    CHECK_EQ(pkt.pad_len, 0);
    CHECK_EQ(pkt.frame_len, 70);
    CHECK_EQ(pkt.frame[12], 0x08);
    CHECK_EQ(pkt.frame[13], 0x00);
    CHECK(packet_parse(pkt.frame, pkt.frame_len, &back, err, (int)sizeof err));
    CHECK_EQ(back.ip_checksum_ok, 1);
    CHECK_EQ(back.tcp_checksum_ok, 1);
    CHECK_EQ(back.tcp.dst_port, 80);
    CHECK_EQ(back.ip.ttl, 64);
    CHECK(memcmp(back.payload, pkt.payload, 16) == 0);
    memcpy(with_fcs, pkt.frame, (size_t)pkt.frame_len);
    memset(with_fcs + pkt.frame_len, 0xab, 4);
    CHECK(packet_parse(with_fcs, pkt.frame_len + 4, &back, err, (int)sizeof err));
    CHECK_EQ(back.fcs_len, 4);
    CHECK_EQ(back.payload_len, 16);
    CHECK_EQ(back.tcp_checksum_ok, 1);
}

static void test_pad(void) {
    Packet pkt;
    Packet back;
    char err[160];

    CHECK(fill_http(&pkt, "Hi"));
    CHECK(parse_tcp_flags("ack", &pkt.tcp.flags));
    CHECK(packet_build(&pkt, err, (int)sizeof err));
    CHECK_EQ(pkt.ip.total_len, 42);
    CHECK_EQ(pkt.pad_len, 4);
    CHECK_EQ(pkt.frame_len, 60);
    CHECK_EQ(pkt.frame[56], 0);
    CHECK_EQ(pkt.frame[59], 0);
    CHECK(packet_parse(pkt.frame, pkt.frame_len, &back, err, (int)sizeof err));
    CHECK_EQ(back.payload_len, 2);
    CHECK_EQ(back.payload[0], 'H');
    CHECK_EQ(back.payload[1], 'i');
    CHECK_EQ(back.tcp_checksum_ok, 1);
    CHECK_EQ(back.fcs_len, 0);
}

static void test_pcap(void) {
    Packet pkt;
    PcapView view;
    unsigned char file[256];
    unsigned int ts_sec = 0;
    unsigned int ts_sub = 0;
    const unsigned char *data = NULL;
    int n = 0;
    int incl = 0;
    int orig = 0;
    char err[160];
    Packet back;

    CHECK(fill_http(&pkt, "Hi"));
    CHECK(packet_build(&pkt, err, (int)sizeof err));
    CHECK(pcap_write_one(file, (int)sizeof file, 3, 4, pkt.frame, pkt.frame_len, &n));
    CHECK_EQ(file[0], 0xd4);
    CHECK_EQ(file[1], 0xc3);
    CHECK_EQ(file[2], 0xb2);
    CHECK_EQ(file[3], 0xa1);
    CHECK(pcap_view(file, n, &view, err, (int)sizeof err));
    CHECK(view.little);
    CHECK_EQ(view.magic, 0xa1b2c3d4u);
    CHECK_EQ(view.network, 1);
    CHECK_EQ(view.count, 1);
    CHECK(pcap_packet(file, n, 0, &ts_sec, &ts_sub, &incl, &orig, &data, err, (int)sizeof err));
    CHECK_EQ(ts_sec, 3);
    CHECK_EQ(ts_sub, 4);
    CHECK_EQ(incl, pkt.frame_len);
    CHECK(packet_parse(data, incl, &back, err, (int)sizeof err));
    CHECK_EQ(back.tcp.dst_port, 80);
    CHECK_STR(ip_proto_name(back.ip.protocol), "TCP");
    CHECK_STR(eth_type_name(back.eth.type), "IPv4");
}

int main(void) {
    test_text();
    test_byte_order();
    test_2011();
    test_roundtrip();
    test_pad();
    test_pcap();
    return TEST_SUMMARY();
}
