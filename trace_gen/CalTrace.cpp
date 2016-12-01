#include <pcap.h>
#include <fstream>
#include <iostream>
#include <getopt.h>
#include <string.h>
#include <unordered_map>
#include <sys/time.h>
#include <vector>
#include "PcapHeader.hpp"

using std::string;
using std::cerr;
using std::cout;
using std::endl;
using std::unordered_map;
using std::vector;
using std::ofstream;

void print_help() {
    cerr << "Usage: FlowEcho {-s src_file -r dst_file}" << endl;
}

typedef struct header_addr {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t ip_id;

    bool operator==(const header_addr &other) const {
        return (src_ip == other.src_ip &&
                dst_ip == other.dst_ip &&
                src_port == other.src_port &&
                dst_port == other.dst_port &&
                ip_id == other.ip_id);
    }
} header_addr;

struct header_hasher {
    std::size_t operator()(const header_addr& hd) const {
        using std::hash;

        return (hash<uint32_t>()(hd.src_ip) ^
                hash<uint32_t>()(hd.dst_ip) ^
                hash<uint16_t>()(hd.src_port) ^
                hash<uint16_t>()(hd.dst_port) ^
                hash<uint16_t>()(hd.ip_id));
    }
};

timeval tv_sub(const timeval & tv1, const timeval & tv2) {
    // asume tv1 > tv2
    int microseconds = (tv1.tv_sec - tv2.tv_sec) * 1000000 + ((int)tv1.tv_usec - (int)tv2.tv_usec);
    timeval tv;
    tv.tv_sec = microseconds/1000000;
    tv.tv_usec = microseconds%1000000;

    return tv;
}

int tv_cmp(const timeval & tv1, const timeval & tv2){
    if (tv1.tv_sec < tv2.tv_sec)
        return -1;
    if (tv1.tv_sec > tv2.tv_sec)
        return 1;

    if (tv1.tv_usec < tv2.tv_usec)
        return -1;
    if (tv1.tv_usec > tv2.tv_usec)
        return 1;
    
    return 0;
}

unordered_map<header_addr, vector<timeval>, header_hasher> record;

void dump_TCP_packet(const unsigned char * packet, struct timeval ts,
                     unsigned int capture_len) {
    if (capture_len < SIZE_ETHERNET) {
        return;
    }

    sniff_ethernet * eth = (sniff_ethernet *)packet;

    if (eth->ether_type != htons(ETHER_TYPE_IP))
        return;

    sniff_ip * ip = (sniff_ip *)(packet + sizeof(sniff_ip));

    header_addr header_val;
    header_val.src_ip = ip->ip_src.s_addr;
    header_val.dst_ip = ip->ip_dst.s_addr;
    header_val.src_port = *(uint16_t*)(eth->ether_shost + 4);
    header_val.dst_port = *(uint16_t*)(eth->ether_dhost + 4);
    header_val.ip_id = ip->ip_id;

    if (record.find(header_val) != record.end()) {
        record[header_val].push_back(ts);
    } else {
        vector<timeval> time_array(1, ts);
        record[header_val] = time_array;
    }
}

int main(int argc, char * argv[]) {
    char src_file[100] = "";
    char rcv_file[100] = "";

    int getopt_res;
    while (1) {
        static struct option parser_options[] = {
            {"help",        no_argument,                0, 'h'},
            {"send",        required_argument,          0, 's'},
            {"recv",        required_argument,          0, 'r'},
            {0,             0,                          0,  0}
        };

        int option_index = 0;

        getopt_res = getopt_long (argc, argv, "hs:r:",
                                  parser_options, &option_index);

        if (getopt_res == -1)
            break;

        switch (getopt_res) {
        case 0:
            if (parser_options[option_index].flag != 0)
                break;
        case 's':
            strcpy(src_file, optarg);
            break;
        case 'r':
            strcpy(rcv_file, optarg);
            break;
        case 'h':
            print_help();
            return 0;
        case '?':
            print_help();
            return 0;
        default:
            abort();
        }
    }

    if (!strcmp(src_file, "") || !strcmp(rcv_file, "")) {
        cout <<"mising input"<<endl;
        print_help();
        return 1;
    }

    // source file
    pcap_t * pd;
    char pebuf[PCAP_ERRBUF_SIZE];
    struct pcap_pkthdr header;
    const unsigned char * packet;

    pd = pcap_open_offline(src_file, pebuf);

    while ((packet = pcap_next(pd, &header))) {
        dump_TCP_packet(packet, header.ts, header.caplen);
    }

    pcap_close(pd);

    // receive file
    pd = pcap_open_offline(rcv_file, pebuf);
    while ((packet = pcap_next(pd, &header))) {
        dump_TCP_packet(packet, header.ts, header.caplen);
    }
    pcap_close(pd);

    ofstream output("latency.data");

    int miss_pkt = 0;
    int total_pkt = 0;

    timeval rtt_min;
    rtt_min.tv_sec = 1;
    rtt_min.tv_usec = 0;
    
    timeval rtt_max;
    rtt_max.tv_sec = 0;
    rtt_max.tv_usec = 0;

    for (const auto rec : record) {
        ++total_pkt;

        if (rec.second.size() != 4) {
            ++miss_pkt;
            continue;
        }

        timeval src_del = tv_sub(rec.second[1], rec.second[0]);
        timeval dst_del = tv_sub(rec.second[3], rec.second[2]);
        timeval rtt = tv_sub(src_del, dst_del);

        output<<rtt.tv_sec<<"."<<rtt.tv_usec<<"\t";
        output<<src_del.tv_sec<<"."<<src_del.tv_usec<<"\t";
        output<<dst_del.tv_sec<<"."<<dst_del.tv_usec<<"\t";

        if (tv_cmp(rtt, rtt_min) < 0)
            rtt_min = rtt;

        if (tv_cmp(rtt, rtt_max) > 0)
            rtt_max = rtt;

        for (const timeval & ts : rec.second) {
            output<<ts.tv_sec<<"."<<ts.tv_usec<<"\t";
        }
        output<<endl;
    }
    output.close();

    cout<<"max rtt: "<<rtt_max.tv_sec<<"."<<rtt_max.tv_usec<<endl;
    cout<<"min rtt: "<<rtt_min.tv_sec<<"."<<rtt_min.tv_usec<<endl;

    return 0;
}
