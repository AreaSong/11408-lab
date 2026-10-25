#include "pcapio.h"

#include <string.h>

#include "bytes.h"
#include "packet.h"
#include "report.h"

static unsigned int rd32(const unsigned char *p, int little) {
    return little ? read_le32(p) : read_be32(p);
}

static unsigned short rd16(const unsigned char *p, int little) {
    return little ? read_le16(p) : read_be16(p);
}

static bool magic_of(unsigned int as_le, int *little, int *nanos, unsigned int *magic) {
    if (as_le == 0xa1b2c3d4u) {
        *little = 1;
        *nanos = 0;
        *magic = as_le;
        return true;
    }
    if (as_le == 0xa1b23c4du) {
        *little = 1;
        *nanos = 1;
        *magic = as_le;
        return true;
    }
    /* 文件若是大端写成的，按小端去读魔数会整字掉个。 */
    if (as_le == 0xd4c3b2a1u) {
        *little = 0;
        *nanos = 0;
        *magic = 0xa1b2c3d4u;
        return true;
    }
    if (as_le == 0x4d3cb2a1u) {
        *little = 0;
        *nanos = 1;
        *magic = 0xa1b23c4du;
        return true;
    }
    return false;
}

static bool fill_header(const unsigned char *buf, int len, PcapView *view, char *err, int err_cap) {
    if (len < 24) {
        snprintf(err, err_cap, "pcap 全局首部要 24 字节，给出 %d 字节", len);
        return false;
    }
    if (!magic_of(read_le32(buf), &view->little, &view->nanos, &view->magic)) {
        snprintf(err, err_cap, "认不出 pcap 魔数 %02x %02x %02x %02x", buf[0], buf[1], buf[2], buf[3]);
        return false;
    }
    view->major = rd16(buf + 4, view->little);
    view->minor = rd16(buf + 6, view->little);
    view->thiszone = (int)rd32(buf + 8, view->little);
    view->sigfigs = rd32(buf + 12, view->little);
    view->snaplen = rd32(buf + 16, view->little);
    view->network = rd32(buf + 20, view->little);
    return true;
}

static const char *link_name(unsigned int network) {
    if (network == 1) {
        return "以太网";
    }
    if (network == 0) {
        return "环回";
    }
    return "其他";
}

bool pcap_write_one(unsigned char *out, int cap, unsigned int ts_sec, unsigned int ts_sub,
                    const unsigned char *frame, int frame_len, int *written) {
    int need = 24 + 16 + frame_len;

    if (frame_len < 0 || need > cap) {
        return false;
    }
    write_le32(out, 0xa1b2c3d4u);
    write_le16(out + 4, 2);
    write_le16(out + 6, 4);
    write_le32(out + 8, 0);
    write_le32(out + 12, 0);
    write_le32(out + 16, 65535u);
    write_le32(out + 20, 1u);
    write_le32(out + 24, ts_sec);
    write_le32(out + 28, ts_sub);
    write_le32(out + 32, (unsigned int)frame_len);
    write_le32(out + 36, (unsigned int)frame_len);
    memcpy(out + 40, frame, (size_t)frame_len);
    *written = need;
    return true;
}

bool pcap_view(const unsigned char *buf, int len, PcapView *view, char *err, int err_cap) {
    int off = 24;

    memset(view, 0, sizeof *view);
    if (!fill_header(buf, len, view, err, err_cap)) {
        return false;
    }
    while (off < len) {
        unsigned int incl;
        if (len - off < 16) {
            snprintf(err, err_cap, "第 %d 个数据包的记录头不完整", view->count + 1);
            return false;
        }
        incl = rd32(buf + off + 8, view->little);
        if (incl > (unsigned int)(len - off - 16)) {
            snprintf(err, err_cap, "第 %d 个数据包被截断", view->count + 1);
            return false;
        }
        off += 16 + (int)incl;
        view->count++;
    }
    return true;
}

bool pcap_packet(const unsigned char *buf, int len, int index, unsigned int *ts_sec,
                 unsigned int *ts_sub, int *incl_len, int *orig_len,
                 const unsigned char **data, char *err, int err_cap) {
    PcapView view;
    int off = 24;
    int i = 0;

    if (!pcap_view(buf, len, &view, err, err_cap)) {
        return false;
    }
    if (index < 0 || index >= view.count) {
        snprintf(err, err_cap, "没有第 %d 个数据包", index + 1);
        return false;
    }
    while (i < index) {
        off += 16 + (int)rd32(buf + off + 8, view.little);
        i++;
    }
    *ts_sec = rd32(buf + off, view.little);
    *ts_sub = rd32(buf + off + 4, view.little);
    *incl_len = (int)rd32(buf + off + 8, view.little);
    *orig_len = (int)rd32(buf + off + 12, view.little);
    *data = buf + off + 16;
    return true;
}

static void report_header(FILE *fp, const unsigned char *buf, const PcapView *view) {
    fprintf(fp, "【pcap 文件】\n");
    fprintf(fp, "前 4 字节 %02x %02x %02x %02x，按%s读出魔数 0x%08x。\n",
            buf[0], buf[1], buf[2], buf[3], view->little ? "小端" : "大端", view->magic);
    if (view->little && view->magic == 0xa1b2c3d4u) {
        fprintf(fp, "这是微秒精度的经典 pcap。小端机器上 tcpdump -w 写成 d4 c3 b2 a1。\n");
    }
    fprintf(fp, "帧里的多字节整数是大端；pcap 文件头用的是写文件那台机器的字节序，两者不是一回事。\n");
    fprintf(fp, "版本 = %u.%u，时区修正 = %d，时间戳精度字段 = %u\n",
            view->major, view->minor, view->thiszone, view->sigfigs);
    fprintf(fp, "抓包快照长度 snaplen = %u 字节\n", view->snaplen);
    fprintf(fp, "链路类型 = %u（%s）。只有 1 才按以太网帧继续拆。\n", view->network, link_name(view->network));
    fprintf(fp, "数据包个数 = %d\n", view->count);
}

static bool report_one(FILE *fp, const PcapView *view, unsigned int ts_sec, unsigned int ts_sub,
                       int incl, int orig, const unsigned char *data, char *err, int err_cap) {
    Packet pkt;
    const char *unit = view->nanos ? "纳秒" : "微秒";

    fprintf(fp, "【数据包】\n");
    fprintf(fp, "时间戳 = %u 秒 + %u %s\n", ts_sec, ts_sub, unit);
    fprintf(fp, "捕获长度 incl_len = %d 字节，原始长度 orig_len = %d 字节\n", incl, orig);
    if (view->network != 1) {
        fprintf(fp, "链路类型不是以太网，不能按以太网首部拆。\n");
        return true;
    }
    if (!packet_parse(data, incl, &pkt, err, err_cap)) {
        return false;
    }
    report_parse(fp, &pkt);
    return true;
}

bool pcap_report(FILE *fp, const unsigned char *buf, int len, char *err, int err_cap) {
    PcapView view;
    int i;
    int shown;

    if (!pcap_view(buf, len, &view, err, err_cap)) {
        return false;
    }
    report_header(fp, buf, &view);
    shown = view.count < 8 ? view.count : 8;
    for (i = 0; i < shown; i++) {
        unsigned int ts_sec = 0;
        unsigned int ts_sub = 0;
        int incl = 0;
        int orig = 0;
        const unsigned char *data = NULL;
        if (i > 0) {
            fputc('\n', fp);
        }
        if (!pcap_packet(buf, len, i, &ts_sec, &ts_sub, &incl, &orig, &data, err, err_cap)) {
            return false;
        }
        if (!report_one(fp, &view, ts_sec, ts_sub, incl, orig, data, err, err_cap)) {
            return false;
        }
    }
    if (view.count > shown) {
        fprintf(fp, "其余 %d 个数据包没有打印。\n", view.count - shown);
    }
    fprintf(fp, "结果：%d 个数据包，链路类型 %s\n", view.count, link_name(view.network));
    return true;
}
