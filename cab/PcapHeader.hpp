#pragma once
#include <sys/types.h>
#include <arpa/inet.h>
#define ETHER_ADDR_LEN	6

/* Ethernet header */
#define ETHER_TYPE_IP 0x0800
struct sniff_ethernet {
    sniff_ethernet():ether_dhost {0},ether_shost {0},ether_type(htons(ETHER_TYPE_IP)) {}
    uint8_t ether_dhost[ETHER_ADDR_LEN]; /* Destination host address */
    uint8_t ether_shost[ETHER_ADDR_LEN]; /* Source host address */
    uint16_t ether_type; /* IP? ARP? RARP? etc */
};
const int SIZE_ETHERNET = sizeof(sniff_ethernet);

/* IP header */
struct sniff_ip {
    sniff_ip()
        :ip_vhl(0x45),
         ip_tos(0x00),
         ip_len(20),ip_id(0),
         ip_off(0x0000),
         ip_ttl(64),
         ip_p(0x06) {
        inet_aton("10.0.0.1",&ip_src);
        inet_aton("10.0.0.2",&ip_dst);
    }
    uint8_t ip_vhl;		/* version << 4 | header length >> 2 */
#define IP_HL(ip)		(((ip)->ip_vhl) & 0x0f)
#define IP_V(ip)		(((ip)->ip_vhl) >> 4)
    uint8_t ip_tos;		/* type of service */
    uint16_t ip_len;		/* total length */
    uint16_t ip_id;		/* identification */
    uint16_t ip_off;		/* fragment offset field */
#define IP_RF 0x8000		/* reserved fragment flag */
#define IP_DF 0x4000		/* dont fragment flag */
#define IP_MF 0x2000		/* more fragments flag */
#define IP_OFFMASK 0x1fff	/* mask for fragmenting bits */
    uint8_t ip_ttl;		/* time to live */
    uint8_t ip_p;		/* protocol */
    uint16_t ip_sum;		/* checksum */
    struct in_addr ip_src,ip_dst; /* source and dest address */
};
const int SIZE_IP = sizeof(sniff_ip);


/* TCP header */
typedef uint32_t tcp_seq;
#define TH_FIN 0x01
#define TH_SYN 0x02
#define TH_RST 0x04
#define TH_PUSH 0x08
#define TH_ACK 0x10
#define TH_URG 0x20
#define TH_ECE 0x40
#define TH_CWR 0x80
#define TH_FLAGS (TH_FIN|TH_SYN|TH_RST|TH_ACK|TH_URG|TH_ECE|TH_CWR)
struct sniff_tcp {
    sniff_tcp():
        th_offx2(0x50), th_flags(TH_ACK),th_sum(0x00),th_urp(0x00) {}
    uint16_t th_sport;	/* source port */
    uint16_t th_dport;	/* destination port */
    tcp_seq th_seq;		/* sequence number */
    tcp_seq th_ack;		/* acknowledgement number */
    uint8_t th_offx2;	/* data offset, rsvd */
#define TH_OFF(th)	(((th)->th_offx2 & 0xf0) >> 4)
    uint8_t th_flags;
    uint16_t th_win;		/* window */
    uint16_t th_sum;		/* checksum */
    uint16_t th_urp;		/* urgent pointer */
};
const int SIZE_TCP = sizeof(sniff_tcp);
