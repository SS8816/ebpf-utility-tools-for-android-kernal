/*
 * pksniff.c — userspace loader + ncurses TUI
 *
 * Features:
 *   • Live scrollable packet list, colour-coded by protocol
 *   • Filter bar  (IP, port, protocol, app-layer tag)
 *   • Packet detail panel with parsed headers + hex dump
 *   • Live stats  (pkt/s, bytes/s, top-5 talkers)
 *   • Save as PCAP  (full or filtered)
 *   • Pause / resume
 *   • BTF fallback  (-b <btf_path>) for kernels without CONFIG_DEBUG_INFO_BTF
 *
 * Build deps: libbpf, libelf, zlib, ncurses
 * Usage: pksniff <ifname> [options]
 *   -b <btf>    path to vmlinux.btf (required on physical Android devices)
 *   -o <pcap>   write all packets to pcap file
 *   -n <bytes>  payload preview bytes (default 256)
 *   -v          verbose: also print to stdout
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ncurses.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include "pksniff.skel.h"
#include "pksniff.h"

/* ── limits ─────────────────────────────────────────────────────────────── */
#define MAX_PACKETS     8192    /* ring of captured events kept in memory    */
#define MAX_TALKERS     5       /* top talkers shown in stats panel          */
#define STATS_INTERVAL  1       /* seconds between stats refresh             */

/* ── colour pair IDs (ncurses) ───────────────────────────────────────────── */
#define COL_HEADER   1
#define COL_TCP      2
#define COL_UDP      3
#define COL_ICMP     4
#define COL_ARP      5
#define COL_DNS      6
#define COL_HTTP     7
#define COL_TLS      8
#define COL_OTHER    9
#define COL_SELECTED 10
#define COL_FILTER   11
#define COL_STATUS   12
#define COL_INGRESS  13
#define COL_EGRESS   14

/* ── filter state ────────────────────────────────────────────────────────── */
struct filter {
    char   ip[INET_ADDRSTRLEN];   /* empty = any                             */
    __u16  port;                  /* 0 = any                                 */
    __u8   proto;                 /* 0 = any (IPPROTO_*)                     */
    __u8   app_tag;               /* 0 = any (APP_*)                         */
    __u8   direction;             /* 0xFF = any, DIR_INGRESS, DIR_EGRESS     */
};

/* ── per-talker stats ────────────────────────────────────────────────────── */
struct talker {
    __u32  ip;
    __u64  bytes;
    __u64  pkts;
};

/* ── global state ────────────────────────────────────────────────────────── */
static struct event  pkt_ring[MAX_PACKETS];
static int           pkt_count    = 0;   /* total captured (wraps ring)       */
static int           pkt_head     = 0;   /* next write index in ring          */
static pthread_mutex_t pkt_mutex  = PTHREAD_MUTEX_INITIALIZER;

static volatile bool g_running    = true;
static volatile bool g_paused     = false;

static struct filter g_filter     = { .direction = 0xFF };
static bool          g_filter_mode = false;   /* are we editing the filter?  */
static char          g_filter_buf[128] = "";  /* raw filter input             */
static int           g_filter_cursor = 0;

static int           g_selected   = 0;        /* highlighted row index        */
static int           g_scroll_top = 0;        /* first visible row            */
static bool          g_detail_mode = false;   /* show detail panel?           */

static FILE         *g_pcap_fp    = NULL;
static bool          g_verbose    = false;

/* stats */
static __u64  g_total_pkts    = 0;
static __u64  g_total_bytes   = 0;
static __u64  g_last_pkts     = 0;
static __u64  g_last_bytes    = 0;
static double g_pps           = 0.0;
static double g_bps           = 0.0;
static struct talker g_talkers[256];     /* hashed by ip&0xFF                */
static struct talker g_top[MAX_TALKERS];
static time_t        g_last_stats_ts = 0;

/* BPF */
static struct pksniff_bpf *g_skel = NULL;
static struct ring_buffer  *g_rb  = NULL;
static struct bpf_tc_hook   g_hook_in = {};
static struct bpf_tc_hook   g_hook_eg = {};
static struct bpf_tc_opts   g_opts_in = {};
static struct bpf_tc_opts   g_opts_eg = {};
static int g_attached_in = 0;
static int g_attached_eg = 0;

/* ncurses windows */
static WINDOW *w_title   = NULL;
static WINDOW *w_filter  = NULL;
static WINDOW *w_list    = NULL;
static WINDOW *w_detail  = NULL;
static WINDOW *w_stats   = NULL;
static WINDOW *w_status  = NULL;

/* ── helpers ─────────────────────────────────────────────────────────────── */
static void sig_handler(int sig) { (void)sig; g_running = false; }

static const char *proto_str(__u8 p) {
    switch (p) {
    case IPPROTO_TCP:  return "TCP";
    case IPPROTO_UDP:  return "UDP";
    case IPPROTO_ICMP: return "ICMP";
    default:           return "???";
    }
}

static const char *app_str(__u8 t) {
    switch (t) {
    case APP_DNS:  return "DNS";
    case APP_HTTP: return "HTTP";
    case APP_TLS:  return "TLS";
    case APP_ICMP: return "ICMP";
    default:       return "";
    }
}

static int col_for_event(const struct event *e) {
    if (e->ip_proto == 0 && e->src_ip == 0)  return COL_ARP;
    switch (e->app_tag) {
    case APP_DNS:  return COL_DNS;
    case APP_HTTP: return COL_HTTP;
    case APP_TLS:  return COL_TLS;
    case APP_ICMP: return COL_ICMP;
    }
    switch (e->ip_proto) {
    case IPPROTO_TCP:  return COL_TCP;
    case IPPROTO_UDP:  return COL_UDP;
    case IPPROTO_ICMP: return COL_ICMP;
    }
    return COL_OTHER;
}

static void ip_str(__u32 ip, char *buf, size_t len) {
    struct in_addr a = { .s_addr = ip };
    inet_ntop(AF_INET, &a, buf, len);
}

/* ── PCAP helpers ────────────────────────────────────────────────────────── */
static void pcap_write_global_header(FILE *fp) {
    struct __attribute__((packed)) {
        uint32_t magic; uint16_t vmaj, vmin;
        int32_t tz; uint32_t sf, snap, net;
    } h = { 0xa1b2c3d4, 2, 4, 0, 0, MAX_CAPTURE_BYTES, 1 };
    fwrite(&h, sizeof(h), 1, fp);
    fflush(fp);
}

static void pcap_write_record(FILE *fp, const struct event *e) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct __attribute__((packed)) {
        uint32_t ts_sec, ts_usec, incl_len, orig_len;
    } r = { (uint32_t)tv.tv_sec, (uint32_t)tv.tv_usec,
            e->cap_len, e->packet_len };
    fwrite(&r, sizeof(r), 1, fp);
    fwrite(e->packet, 1, r.incl_len, fp);
    fflush(fp);
}

/* ── filter matching ─────────────────────────────────────────────────────── */
static bool event_matches(const struct event *e) {
    if (g_filter.ip[0]) {
        char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];
        ip_str(e->src_ip, src, sizeof(src));
        ip_str(e->dst_ip, dst, sizeof(dst));
        if (strstr(src, g_filter.ip) == NULL &&
            strstr(dst, g_filter.ip) == NULL)
            return false;
    }
    if (g_filter.port) {
        if (e->src_port != g_filter.port && e->dst_port != g_filter.port)
            return false;
    }
    if (g_filter.proto) {
        if (e->ip_proto != g_filter.proto) return false;
    }
    if (g_filter.app_tag) {
        if (e->app_tag != g_filter.app_tag) return false;
    }
    if (g_filter.direction != 0xFF) {
        if (e->direction != g_filter.direction) return false;
    }
    return true;
}

/*
 * parse_filter_str — parse a simple filter string.
 * Supported tokens: ip:<addr>, port:<n>, proto:<tcp|udp|icmp>,
 *                   app:<dns|http|tls>, dir:<in|out>
 */
static void parse_filter_str(const char *s) {
    memset(&g_filter, 0, sizeof(g_filter));
    g_filter.direction = 0xFF;

    char buf[128] = {};
    strncpy(buf, s, sizeof(buf) - 1);
    char *tok = strtok(buf, " ");
    while (tok) {
        if (strncmp(tok, "ip:", 3) == 0) {
            strncpy(g_filter.ip, tok + 3, sizeof(g_filter.ip) - 1);
        } else if (strncmp(tok, "port:", 5) == 0) {
            g_filter.port = (__u16)atoi(tok + 5);
        } else if (strncmp(tok, "proto:", 6) == 0) {
            const char *p = tok + 6;
            if      (strcasecmp(p, "tcp")  == 0) g_filter.proto = IPPROTO_TCP;
            else if (strcasecmp(p, "udp")  == 0) g_filter.proto = IPPROTO_UDP;
            else if (strcasecmp(p, "icmp") == 0) g_filter.proto = IPPROTO_ICMP;
        } else if (strncmp(tok, "app:", 4) == 0) {
            const char *p = tok + 4;
            if      (strcasecmp(p, "dns")  == 0) g_filter.app_tag = APP_DNS;
            else if (strcasecmp(p, "http") == 0) g_filter.app_tag = APP_HTTP;
            else if (strcasecmp(p, "tls")  == 0) g_filter.app_tag = APP_TLS;
        } else if (strncmp(tok, "dir:", 4) == 0) {
            const char *p = tok + 4;
            if      (strcasecmp(p, "in")  == 0) g_filter.direction = DIR_INGRESS;
            else if (strcasecmp(p, "out") == 0) g_filter.direction = DIR_EGRESS;
        }
        tok = strtok(NULL, " ");
    }
}

/* ── stats update ────────────────────────────────────────────────────────── */
static void update_stats(const struct event *e) {
    g_total_pkts++;
    g_total_bytes += e->packet_len;

    /* simple hash by last octet of src IP */
    int slot = e->src_ip & 0xFF;
    if (g_talkers[slot].ip == 0 || g_talkers[slot].ip == e->src_ip) {
        g_talkers[slot].ip    = e->src_ip;
        g_talkers[slot].bytes += e->packet_len;
        g_talkers[slot].pkts++;
    }

    time_t now = time(NULL);
    if (now - g_last_stats_ts >= STATS_INTERVAL) {
        g_pps = (double)(g_total_pkts - g_last_pkts);
        g_bps = (double)(g_total_bytes - g_last_bytes);
        g_last_pkts  = g_total_pkts;
        g_last_bytes = g_total_bytes;
        g_last_stats_ts = now;

        /* find top talkers */
        memset(g_top, 0, sizeof(g_top));
        for (int i = 0; i < 256; i++) {
            if (!g_talkers[i].ip) continue;
            for (int j = 0; j < MAX_TALKERS; j++) {
                if (g_talkers[i].bytes > g_top[j].bytes) {
                    /* shift down */
                    for (int k = MAX_TALKERS - 1; k > j; k--)
                        g_top[k] = g_top[k-1];
                    g_top[j] = g_talkers[i];
                    break;
                }
            }
        }
    }
}

/* ── ring-buffer event callback ──────────────────────────────────────────── */
static int handle_event(void *ctx, void *data, size_t size)
{
    (void)ctx; (void)size;
    if (g_paused) return 0;

    struct event *e = data;

    if (g_pcap_fp) pcap_write_record(g_pcap_fp, e);

    pthread_mutex_lock(&pkt_mutex);
    pkt_ring[pkt_head] = *e;
    pkt_head = (pkt_head + 1) % MAX_PACKETS;
    if (pkt_count < MAX_PACKETS) pkt_count++;
    update_stats(e);
    pthread_mutex_unlock(&pkt_mutex);

    return 0;
}

/* ════════════════════════════════════════════════════════════════════════════
 * ncurses UI
 * ══════════════════════════════════════════════════════════════════════════ */

static void init_colors(void) {
    start_color();
    use_default_colors();
    init_pair(COL_HEADER,   COLOR_BLACK,   COLOR_CYAN);
    init_pair(COL_TCP,      COLOR_GREEN,   -1);
    init_pair(COL_UDP,      COLOR_YELLOW,  -1);
    init_pair(COL_ICMP,     COLOR_MAGENTA, -1);
    init_pair(COL_ARP,      COLOR_CYAN,    -1);
    init_pair(COL_DNS,      COLOR_BLUE,    -1);
    init_pair(COL_HTTP,     COLOR_WHITE,   COLOR_BLUE);
    init_pair(COL_TLS,      COLOR_WHITE,   COLOR_GREEN);
    init_pair(COL_OTHER,    COLOR_WHITE,   -1);
    init_pair(COL_SELECTED, COLOR_BLACK,   COLOR_WHITE);
    init_pair(COL_FILTER,   COLOR_BLACK,   COLOR_YELLOW);
    init_pair(COL_STATUS,   COLOR_BLACK,   COLOR_GREEN);
    init_pair(COL_INGRESS,  COLOR_CYAN,    -1);
    init_pair(COL_EGRESS,   COLOR_YELLOW,  -1);
}

static void layout_windows(void) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    /* title: 1 row at top */
    if (w_title)  { delwin(w_title);  w_title  = NULL; }
    if (w_filter) { delwin(w_filter); w_filter = NULL; }
    if (w_list)   { delwin(w_list);   w_list   = NULL; }
    if (w_detail) { delwin(w_detail); w_detail = NULL; }
    if (w_stats)  { delwin(w_stats);  w_stats  = NULL; }
    if (w_status) { delwin(w_status); w_status = NULL; }

    w_title  = newwin(1, cols, 0, 0);
    w_filter = newwin(1, cols, 1, 0);

    int stats_h = 8;
    int list_cols = g_detail_mode ? cols * 6 / 10 : cols;
    int list_rows = rows - 4 - stats_h;
    if (list_rows < 4) list_rows = 4;

    w_list  = newwin(list_rows, list_cols, 2, 0);

    if (g_detail_mode) {
        int dc = cols - list_cols;
        w_detail = newwin(list_rows, dc, 2, list_cols);
    }

    w_stats  = newwin(stats_h, cols, 2 + list_rows, 0);
    w_status = newwin(1, cols, rows - 1, 0);
}

static void draw_title(const char *ifname, bool paused) {
    wbkgd(w_title, COLOR_PAIR(COL_HEADER));
    werase(w_title);
    int cols = getmaxx(w_title);
    char buf[256];
    snprintf(buf, sizeof(buf),
             " pksniff — %s%s  [F: filter  D: detail  P: pause  S: save  Q: quit]",
             ifname, paused ? "  *** PAUSED ***" : "");
    wattron(w_title, COLOR_PAIR(COL_HEADER) | A_BOLD);
    mvwprintw(w_title, 0, 0, "%-*s", cols, buf);
    wattroff(w_title, COLOR_PAIR(COL_HEADER) | A_BOLD);
    wrefresh(w_title);
}

static void draw_filter_bar(void) {
    werase(w_filter);
    int cols = getmaxx(w_filter);

    if (g_filter_mode) {
        wattron(w_filter, COLOR_PAIR(COL_FILTER) | A_BOLD);
        wbkgd(w_filter, COLOR_PAIR(COL_FILTER));
        mvwprintw(w_filter, 0, 0, " Filter> %-*s", cols - 10, g_filter_buf);
        wmove(w_filter, 0, 9 + g_filter_cursor);
        wattroff(w_filter, COLOR_PAIR(COL_FILTER) | A_BOLD);
    } else {
        /* show active filter summary */
        char active[256] = "No filter";
        if (g_filter.ip[0] || g_filter.port || g_filter.proto ||
            g_filter.app_tag || g_filter.direction != 0xFF) {
            snprintf(active, sizeof(active), "Filter: %s%s%s%s%s%s%s%s%s%s",
                g_filter.ip[0]   ? "ip:" : "", g_filter.ip,
                g_filter.port    ? " port:" : "",
                g_filter.port    ? (char[8]){} : "",
                g_filter.proto   ? " proto:" : "",
                g_filter.proto   ? proto_str(g_filter.proto) : "",
                g_filter.app_tag ? " app:" : "",
                g_filter.app_tag ? app_str(g_filter.app_tag) : "",
                g_filter.direction != 0xFF ? " dir:" : "",
                g_filter.direction == DIR_INGRESS ? "in" :
                g_filter.direction == DIR_EGRESS  ? "out" : "");
        }
        /* rebuild cleanly */
        char summary[256];
        summary[0] = '\0';
        if (g_filter.ip[0]) { strncat(summary, "ip:", 3); strncat(summary, g_filter.ip, sizeof(summary)-strlen(summary)-1); }
        if (g_filter.port)  { char tmp[16]; snprintf(tmp,16," port:%u",g_filter.port); strncat(summary,tmp,sizeof(summary)-strlen(summary)-1); }
        if (g_filter.proto) { char tmp[16]; snprintf(tmp,16," proto:%s",proto_str(g_filter.proto)); strncat(summary,tmp,sizeof(summary)-strlen(summary)-1); }
        if (g_filter.app_tag){ char tmp[16]; snprintf(tmp,16," app:%s",app_str(g_filter.app_tag)); strncat(summary,tmp,sizeof(summary)-strlen(summary)-1); }
        if (g_filter.direction != 0xFF) {
            strncat(summary, g_filter.direction == DIR_INGRESS ? " dir:in" : " dir:out",
                    sizeof(summary)-strlen(summary)-1);
        }

        wattron(w_filter, A_DIM);
        mvwprintw(w_filter, 0, 0, " [Filter: %-*s]  Syntax: ip:<addr> port:<n> proto:<tcp|udp|icmp> app:<dns|http|tls> dir:<in|out>",
                  20, summary[0] ? summary : "none");
        wattroff(w_filter, A_DIM);
    }
    wrefresh(w_filter);
}

static void draw_packet_list(void) {
    int rows = getmaxy(w_list);
    int cols = getmaxx(w_list);
    werase(w_list);

    /* column header */
    wattron(w_list, A_REVERSE);
    mvwprintw(w_list, 0, 0, "%-4s %-3s %-3s %-15s %-5s %-15s %-5s %6s %4s  %s",
              "#", "Dir", "Pro", "SRC", "SPORT", "DST", "DPORT", "Bytes", "App", "Info");
    wattroff(w_list, A_REVERSE);

    pthread_mutex_lock(&pkt_mutex);
    int total = pkt_count;
    pthread_mutex_unlock(&pkt_mutex);

    /* build filtered index list on the fly */
    /* (for large captures we'd cache this, fine for 8K) */
    int visible[MAX_PACKETS];
    int vcount = 0;

    pthread_mutex_lock(&pkt_mutex);
    for (int i = 0; i < total && vcount < MAX_PACKETS; i++) {
        int idx = (pkt_head - total + i + MAX_PACKETS) % MAX_PACKETS;
        if (event_matches(&pkt_ring[idx]))
            visible[vcount++] = idx;
    }
    pthread_mutex_unlock(&pkt_mutex);

    /* clamp selection */
    if (g_selected >= vcount) g_selected = vcount > 0 ? vcount - 1 : 0;
    if (g_scroll_top > g_selected) g_scroll_top = g_selected;
    if (g_selected >= g_scroll_top + rows - 1)
        g_scroll_top = g_selected - rows + 2;

    for (int row = 1; row < rows; row++) {
        int vi = g_scroll_top + row - 1;
        if (vi >= vcount) break;

        pthread_mutex_lock(&pkt_mutex);
        struct event e = pkt_ring[visible[vi]];
        pthread_mutex_unlock(&pkt_mutex);

        char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];
        ip_str(e.src_ip, src, sizeof(src));
        ip_str(e.dst_ip, dst, sizeof(dst));

        /* direction indicator */
        const char *dir_s = e.direction == DIR_INGRESS ? "IN " : "OUT";
        /* protocol */
        const char *proto_s;
        if (e.ip_proto == 0 && e.src_ip == 0)
            proto_s = "ARP";
        else
            proto_s = proto_str(e.ip_proto);

        const char *app_s = app_str(e.app_tag);

        /* extra info snippet */
        char info[48] = "";
        if (e.app_tag == APP_DNS  && e.dns_name[0])
            snprintf(info, sizeof(info), "%s", e.dns_name);
        else if (e.app_tag == APP_HTTP && e.http_info[0])
            snprintf(info, sizeof(info), "%s", e.http_info);
        else if (e.app_tag == APP_TLS && e.tls_sni[0])
            snprintf(info, sizeof(info), "SNI:%s", e.tls_sni);
        else if (e.ip_proto == IPPROTO_TCP) {
            /* show TCP flags */
            snprintf(info, sizeof(info), "[%s%s%s%s%s%s]",
                (e.tcp_flags & TF_SYN) ? "S" : "",
                (e.tcp_flags & TF_ACK) ? "A" : "",
                (e.tcp_flags & TF_FIN) ? "F" : "",
                (e.tcp_flags & TF_RST) ? "R" : "",
                (e.tcp_flags & TF_PSH) ? "P" : "",
                (e.tcp_flags & TF_URG) ? "U" : "");
        }

        int col = col_for_event(&e);
        if (vi == g_selected) col = COL_SELECTED;

        wattron(w_list, COLOR_PAIR(col));
        mvwprintw(w_list, row, 0, "%-4d %-3s %-3s %-15.15s %-5u %-15.15s %-5u %6u %-4s  %-*.*s",
                  vi + 1, dir_s, proto_s,
                  src, e.src_port,
                  dst, e.dst_port,
                  e.packet_len,
                  app_s,
                  cols - 72, cols - 72, info);
        wattroff(w_list, COLOR_PAIR(col));
    }

    wrefresh(w_list);
}

static void draw_detail(void) {
    if (!w_detail) return;

    int rows = getmaxy(w_detail);
    int cols = getmaxx(w_detail);
    werase(w_detail);
    box(w_detail, 0, 0);

    /* get selected event */
    int total;
    pthread_mutex_lock(&pkt_mutex); total = pkt_count; pthread_mutex_unlock(&pkt_mutex);

    int vcount = 0, sel_idx = -1;
    pthread_mutex_lock(&pkt_mutex);
    for (int i = 0; i < total; i++) {
        int idx = (pkt_head - total + i + MAX_PACKETS) % MAX_PACKETS;
        if (event_matches(&pkt_ring[idx])) {
            if (vcount == g_selected) { sel_idx = idx; break; }
            vcount++;
        }
    }
    if (sel_idx < 0) { pthread_mutex_unlock(&pkt_mutex); wrefresh(w_detail); return; }
    struct event e = pkt_ring[sel_idx];
    pthread_mutex_unlock(&pkt_mutex);

    char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];
    ip_str(e.src_ip, src, sizeof(src));
    ip_str(e.dst_ip, dst, sizeof(dst));

    int row = 1;
    wattron(w_detail, A_BOLD);
    mvwprintw(w_detail, row++, 1, "─── Packet Detail ───────────────────");
    wattroff(w_detail, A_BOLD);

    mvwprintw(w_detail, row++, 1, "Dir:      %s",
              e.direction == DIR_INGRESS ? "INGRESS" : "EGRESS");
    mvwprintw(w_detail, row++, 1, "Proto:    %s (0x%02x)",
              e.ip_proto == 0 && e.src_ip == 0 ? "ARP" : proto_str(e.ip_proto),
              e.ip_proto);
    mvwprintw(w_detail, row++, 1, "Src:      %s:%u", src, e.src_port);
    mvwprintw(w_detail, row++, 1, "Dst:      %s:%u", dst, e.dst_port);
    mvwprintw(w_detail, row++, 1, "TTL:      %u", e.ttl);
    mvwprintw(w_detail, row++, 1, "IP len:   %u  (cap %u)", e.packet_len, e.cap_len);
    mvwprintw(w_detail, row++, 1, "Payload:  %u bytes", e.payload_len);

    if (e.ip_proto == IPPROTO_TCP) {
        mvwprintw(w_detail, row++, 1, "TCP flags:[%s%s%s%s%s%s]",
            (e.tcp_flags & TF_SYN) ? "SYN " : "",
            (e.tcp_flags & TF_ACK) ? "ACK " : "",
            (e.tcp_flags & TF_FIN) ? "FIN " : "",
            (e.tcp_flags & TF_RST) ? "RST " : "",
            (e.tcp_flags & TF_PSH) ? "PSH " : "",
            (e.tcp_flags & TF_URG) ? "URG " : "");
    }

    if (e.app_tag) {
        row++;
        wattron(w_detail, A_BOLD);
        mvwprintw(w_detail, row++, 1, "─── App Layer (%s)", app_str(e.app_tag));
        wattroff(w_detail, A_BOLD);
        if (e.dns_name[0])  mvwprintw(w_detail, row++, 1, "DNS:  %s", e.dns_name);
        if (e.http_info[0]) mvwprintw(w_detail, row++, 1, "HTTP: %s", e.http_info);
        if (e.tls_sni[0])   mvwprintw(w_detail, row++, 1, "SNI:  %s", e.tls_sni);
    }

    /* hex dump */
    row++;
    if (row < rows - 2) {
        wattron(w_detail, A_BOLD);
        mvwprintw(w_detail, row++, 1, "─── Hex Dump (first %u bytes)", e.cap_len);
        wattroff(w_detail, A_BOLD);

        int bytes_per_row = (cols - 6) / 3;
        if (bytes_per_row < 8) bytes_per_row = 8;
        if (bytes_per_row > 16) bytes_per_row = 16;

        for (__u32 off = 0; off < e.cap_len && row < rows - 1; off += bytes_per_row) {
            char hex[64] = "", asc[20] = "";
            int hl = 0, al = 0;
            for (int b = 0; b < bytes_per_row && off + b < e.cap_len; b++) {
                __u8 byte = e.packet[off + b];
                hl += snprintf(hex + hl, sizeof(hex) - hl, "%02x ", byte);
                asc[al++] = (byte >= 32 && byte < 127) ? (char)byte : '.';
            }
            asc[al] = '\0';
            mvwprintw(w_detail, row++, 1, "%04x  %-*s  %s",
                      off, bytes_per_row * 3, hex, asc);
        }
    }

    wrefresh(w_detail);
}

static void draw_stats(void) {
    werase(w_stats);
    int cols = getmaxx(w_stats);
    box(w_stats, 0, 0);

    pthread_mutex_lock(&pkt_mutex);
    __u64 tot_p = g_total_pkts, tot_b = g_total_bytes;
    double pps  = g_pps, bps = g_bps;
    pthread_mutex_unlock(&pkt_mutex);

    wattron(w_stats, A_BOLD);
    mvwprintw(w_stats, 0, 2, "[ Statistics ]");
    wattroff(w_stats, A_BOLD);

    mvwprintw(w_stats, 1, 2, "Total packets: %-10llu  Total bytes: %-12llu",
              (unsigned long long)tot_p, (unsigned long long)tot_b);
    mvwprintw(w_stats, 2, 2, "Rate:  %.0f pkt/s   %.0f B/s (%.1f KB/s)",
              pps, bps, bps / 1024.0);

    mvwprintw(w_stats, 3, 2, "Top talkers:");
    pthread_mutex_lock(&pkt_mutex);
    for (int i = 0; i < MAX_TALKERS; i++) {
        if (!g_top[i].ip) break;
        char ips[INET_ADDRSTRLEN];
        ip_str(g_top[i].ip, ips, sizeof(ips));
        mvwprintw(w_stats, 4 + i, 4, "%-16s  %6llu pkts  %8llu B",
                  ips,
                  (unsigned long long)g_top[i].pkts,
                  (unsigned long long)g_top[i].bytes);
    }
    pthread_mutex_unlock(&pkt_mutex);

    (void)cols;
    wrefresh(w_stats);
}

static void draw_status(const char *msg) {
    werase(w_status);
    int cols = getmaxx(w_status);
    wbkgd(w_status, COLOR_PAIR(COL_STATUS));
    wattron(w_status, COLOR_PAIR(COL_STATUS));
    mvwprintw(w_status, 0, 0, "%-*s", cols,
              msg ? msg : " ↑↓:scroll  Enter:detail  F:filter  P:pause  S:save pcap  R:reset  Q:quit");
    wattroff(w_status, COLOR_PAIR(COL_STATUS));
    wrefresh(w_status);
}

static void redraw_all(const char *ifname) {
    int total;
    pthread_mutex_lock(&pkt_mutex); total = pkt_count; pthread_mutex_unlock(&pkt_mutex);
    draw_title(ifname, g_paused);
    draw_filter_bar();
    draw_packet_list();
    if (g_detail_mode) draw_detail();
    draw_stats();
    if (g_filter_mode)
        draw_status("Filter syntax: ip:<addr> port:<n> proto:<tcp|udp|icmp> app:<dns|http|tls> dir:<in|out>  Enter:apply  Esc:cancel");
    else
        draw_status(NULL);
    (void)total;
}

/* ── save filtered PCAP ──────────────────────────────────────────────────── */
static void save_filtered_pcap(const char *path) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return;
    pcap_write_global_header(fp);

    pthread_mutex_lock(&pkt_mutex);
    int total = pkt_count;
    for (int i = 0; i < total; i++) {
        int idx = (pkt_head - total + i + MAX_PACKETS) % MAX_PACKETS;
        if (event_matches(&pkt_ring[idx]))
            pcap_write_record(fp, &pkt_ring[idx]);
    }
    pthread_mutex_unlock(&pkt_mutex);
    fclose(fp);
}

/* ── input handling ──────────────────────────────────────────────────────── */
static void handle_input_normal(int ch, const char *ifname) {
    int total;
    pthread_mutex_lock(&pkt_mutex); total = pkt_count; pthread_mutex_unlock(&pkt_mutex);

    int vcount = 0;
    pthread_mutex_lock(&pkt_mutex);
    for (int i = 0; i < total; i++) {
        int idx = (pkt_head - total + i + MAX_PACKETS) % MAX_PACKETS;
        if (event_matches(&pkt_ring[idx])) vcount++;
    }
    pthread_mutex_unlock(&pkt_mutex);

    switch (ch) {
    case 'q': case 'Q':
        g_running = false;
        break;
    case 'p': case 'P':
        g_paused = !g_paused;
        break;
    case 'f': case 'F':
        g_filter_mode = true;
        strncpy(g_filter_buf, g_filter_buf[0] ? g_filter_buf : "", sizeof(g_filter_buf));
        g_filter_cursor = strlen(g_filter_buf);
        break;
    case 'd': case 'D': case '\n': case KEY_ENTER:
        g_detail_mode = !g_detail_mode;
        layout_windows();
        break;
    case 'r': case 'R':
        memset(&g_filter, 0, sizeof(g_filter));
        g_filter.direction = 0xFF;
        g_filter_buf[0] = '\0';
        g_filter_cursor = 0;
        break;
    case 's': case 'S': {
        /* save filtered pcap with timestamped name */
        char path[64];
        time_t t = time(NULL);
        struct tm *tm_info = localtime(&t);
        strftime(path, sizeof(path), "pksniff_%Y%m%d_%H%M%S.pcap", tm_info);
        save_filtered_pcap(path);
        draw_status(path);   /* briefly show saved path */
        break;
    }
    case KEY_UP:   case 'k':
        if (g_selected > 0) g_selected--;
        break;
    case KEY_DOWN: case 'j':
        if (g_selected < vcount - 1) g_selected++;
        break;
    case KEY_PPAGE:
        g_selected -= getmaxy(w_list) - 2;
        if (g_selected < 0) g_selected = 0;
        break;
    case KEY_NPAGE:
        g_selected += getmaxy(w_list) - 2;
        if (g_selected >= vcount) g_selected = vcount > 0 ? vcount - 1 : 0;
        break;
    case KEY_HOME: g_selected = 0; break;
    case KEY_END:  g_selected = vcount > 0 ? vcount - 1 : 0; break;
    case KEY_RESIZE:
        layout_windows();
        break;
    }
    (void)ifname;
}

static void handle_input_filter(int ch) {
    int len = strlen(g_filter_buf);
    switch (ch) {
    case 27:            /* Esc — cancel */
        g_filter_mode = false;
        break;
    case '\n': case KEY_ENTER:
        parse_filter_str(g_filter_buf);
        g_selected    = 0;
        g_scroll_top  = 0;
        g_filter_mode = false;
        break;
    case KEY_BACKSPACE: case 127: case '\b':
        if (g_filter_cursor > 0) {
            memmove(g_filter_buf + g_filter_cursor - 1,
                    g_filter_buf + g_filter_cursor,
                    len - g_filter_cursor + 1);
            g_filter_cursor--;
        }
        break;
    case KEY_LEFT:
        if (g_filter_cursor > 0) g_filter_cursor--;
        break;
    case KEY_RIGHT:
        if (g_filter_cursor < len) g_filter_cursor++;
        break;
    case KEY_DC:   /* Delete */
        if (g_filter_cursor < len) {
            memmove(g_filter_buf + g_filter_cursor,
                    g_filter_buf + g_filter_cursor + 1,
                    len - g_filter_cursor);
        }
        break;
    default:
        if (ch >= 32 && ch < 127 && len < (int)sizeof(g_filter_buf) - 1) {
            memmove(g_filter_buf + g_filter_cursor + 1,
                    g_filter_buf + g_filter_cursor,
                    len - g_filter_cursor + 1);
            g_filter_buf[g_filter_cursor++] = (char)ch;
        }
        break;
    }
}

/* ── usage ───────────────────────────────────────────────────────────────── */
static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s <ifname> [options]\n"
        "Options:\n"
        "  -b <btf_path>   path to vmlinux.btf (required on physical Android devices\n"
        "                  without CONFIG_DEBUG_INFO_BTF)\n"
        "  -o <pcap>       write ALL packets to pcap (raw, no filter)\n"
        "  -v              verbose — also log events to stdout\n"
        "  -h              help\n"
        "\n"
        "Filter syntax (press F in UI):\n"
        "  ip:<addr>  port:<n>  proto:<tcp|udp|icmp>  app:<dns|http|tls>  dir:<in|out>\n"
        "  Tokens are AND-ed.  Example: proto:tcp port:443 dir:out\n"
        "\n"
        "Keys:\n"
        "  F  open/close filter bar     R  reset filter\n"
        "  D  toggle detail panel       S  save filtered view as pcap\n"
        "  P  pause/resume              Q  quit\n"
        "  ↑↓/j/k  scroll  PgUp/PgDn   Home/End\n",
        prog);
}

/* ════════════════════════════════════════════════════════════════════════════
 * main
 * ══════════════════════════════════════════════════════════════════════════ */
int main(int argc, char **argv)
{
    const char *ifname    = NULL;
    const char *btf_path  = NULL;
    const char *pcap_path = NULL;
    int err = 0;

    static const struct option long_opts[] = {
        {"btf",   required_argument, NULL, 'b'},
        {"out",   required_argument, NULL, 'o'},
        {"verbose", no_argument,     NULL, 'v'},
        {"help",  no_argument,       NULL, 'h'},
        {0, 0, 0, 0},
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "b:o:vh", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'b': btf_path  = optarg; break;
        case 'o': pcap_path = optarg; break;
        case 'v': g_verbose = true;   break;
        default:  usage(argv[0]);     return 1;
        }
    }
    if (optind < argc) ifname = argv[optind];
    if (!ifname) { usage(argv[0]); return 1; }

    int ifindex = if_nametoindex(ifname);
    if (!ifindex) { perror("if_nametoindex"); return 1; }

    /* ── PCAP output ─────────────────────────────────────────────────────  */
    if (pcap_path) {
        g_pcap_fp = fopen(pcap_path, "wb");
        if (!g_pcap_fp) { perror("fopen pcap"); return 1; }
        pcap_write_global_header(g_pcap_fp);
    }

    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    /* ── load BPF skeleton ───────────────────────────────────────────────  */
    struct bpf_object_open_opts open_opts = {
        .sz = sizeof(open_opts),
    };
    if (btf_path) {
        open_opts.btf_custom_path = btf_path;
        fprintf(stderr, "Using custom BTF: %s\n", btf_path);
    }

    g_skel = pksniff_bpf__open_opts(&open_opts);
    if (!g_skel) {
        fprintf(stderr,
            "Failed to open BPF skeleton.\n"
            "If the error is 'kernel BTF is missing', the device kernel was not\n"
            "built with CONFIG_DEBUG_INFO_BTF.  Generate a vmlinux.btf file for\n"
            "this kernel and pass it with:  -b /path/to/vmlinux.btf\n"
            "\n"
            "Quick guide to generate vmlinux.btf on the device:\n"
            "  1) On a host with matching kernel headers, run:\n"
            "       pahole --btf_encode_detached vmlinux.btf vmlinux\n"
            "  2) Push to device:  adb push vmlinux.btf /data/local/tmp/\n"
            "  3) Run:  pksniff %s -b /data/local/tmp/vmlinux.btf\n",
            ifname);
        return 1;
    }

    err = pksniff_bpf__load(g_skel);
    if (err) {
        fprintf(stderr, "Failed to load BPF programs: %d\n", err);
        pksniff_bpf__destroy(g_skel);
        return 1;
    }

    /* ── attach TC ingress ───────────────────────────────────────────────  */
    g_hook_in.sz           = sizeof(g_hook_in);
    g_hook_in.ifindex      = ifindex;
    g_hook_in.attach_point = BPF_TC_INGRESS;

    err = bpf_tc_hook_create(&g_hook_in);
    if (err && err != -EEXIST) {
        fprintf(stderr, "Failed to create ingress TC hook: %d\n", err);
        goto cleanup;
    }

    g_opts_in.sz       = sizeof(g_opts_in);
    g_opts_in.prog_fd  = bpf_program__fd(g_skel->progs.pksniff_ingress);
    g_opts_in.priority = 1;
    g_opts_in.handle   = 1;

    err = bpf_tc_attach(&g_hook_in, &g_opts_in);
    if (err) { fprintf(stderr, "Failed to attach ingress TC: %d\n", err); goto cleanup; }
    g_attached_in = 1;

    /* ── attach TC egress ────────────────────────────────────────────────  */
    g_hook_eg.sz           = sizeof(g_hook_eg);
    g_hook_eg.ifindex      = ifindex;
    g_hook_eg.attach_point = BPF_TC_EGRESS;

    err = bpf_tc_hook_create(&g_hook_eg);
    if (err && err != -EEXIST) {
        fprintf(stderr, "Failed to create egress TC hook: %d\n", err);
        goto cleanup;
    }

    g_opts_eg.sz       = sizeof(g_opts_eg);
    g_opts_eg.prog_fd  = bpf_program__fd(g_skel->progs.pksniff_egress);
    g_opts_eg.priority = 1;
    g_opts_eg.handle   = 1;

    err = bpf_tc_attach(&g_hook_eg, &g_opts_eg);
    if (err) { fprintf(stderr, "Failed to attach egress TC: %d\n", err); goto cleanup; }
    g_attached_eg = 1;

    /* ── ring buffer ─────────────────────────────────────────────────────  */
    g_rb = ring_buffer__new(bpf_map__fd(g_skel->maps.rb), handle_event, NULL, NULL);
    if (!g_rb) { fprintf(stderr, "Failed to create ring buffer\n"); goto cleanup; }

    /* ── ncurses init ────────────────────────────────────────────────────  */
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    timeout(50);   /* non-blocking getch with 50ms timeout */
    curs_set(0);
    if (has_colors()) init_colors();

    layout_windows();
    redraw_all(ifname);

    /* ── main event loop ─────────────────────────────────────────────────  */
    while (g_running) {
        /* poll BPF ring buffer (non-blocking) */
        err = ring_buffer__poll(g_rb, 0);
        if (err < 0 && err != -EINTR) break;

        /* handle keyboard */
        int ch = getch();
        if (ch != ERR) {
            if (g_filter_mode)
                handle_input_filter(ch);
            else
                handle_input_normal(ch, ifname);
        }

        redraw_all(ifname);
    }

    /* teardown ncurses */
    endwin();

cleanup:
    if (g_rb)          ring_buffer__free(g_rb);
    if (g_attached_eg) { bpf_tc_detach(&g_hook_eg, &g_opts_eg); bpf_tc_hook_destroy(&g_hook_eg); }
    if (g_attached_in) { bpf_tc_detach(&g_hook_in, &g_opts_in); bpf_tc_hook_destroy(&g_hook_in); }
    if (g_pcap_fp)     fclose(g_pcap_fp);
    pksniff_bpf__destroy(g_skel);

    return err < 0 ? -err : 0;
}