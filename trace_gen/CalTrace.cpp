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


void print_help() {
    cerr << "Usage: FlowEcho {-s src_file -r dst_file}" << endl;
}

typedef struct header_addr {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t chksum;

    bool operator==(const header_addr &other) const {
        return (src_ip == other.src_ip && 
                dst_ip == other.dst_ip &&
                src_port == other.src_port &&
                dst_port == other.dst_port &&
                chksum == other.chksum);
    }
} header_addr;

unordered_map<header_addr, vector<timeval> > record;

void dump_TCP_packet(const unsigned char * packet, struct timeval ts,
                     unsigned int capture_len) {
    if (capture_len < SIZE_ETHERNET){
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

    if (record.find(header_val) != record.end()){
        record[header_val].second.push_back(ts);
    }    
    else{
        vector<timeval> time_array(1, ts);
        record[header_val].insert(std::make_pair(header_val, time_array));
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
        print_help();
        return 0;
    }

    // first file
    pcap_t * pd;
    char pebuf[PCAP_ERRBUF_SIZE];
    struct pcap_pkthdr header;
    const unsigned char * packet;

    pd = pcap_open_offline(src_file, pebuf);

    while ((packet = pcap_next(pd, &header))) {
        dump_TCP_packet(packet, header.ts, header.caplen);
    }

    pcap_close(pd);

    // seoncd file 
    /* pd = pcap_open_offline(rcv_file, pebuf);
     * 
     * while ((packet = pcap_next(pd, &header))) {
     *     dump_TCP_packet(packet, header.ts, header.caplen);
     * }
     * pcap_close(pd); */

    for (const timeval & rec : record) {
        for (const timeval & ts : rec.pair){
            cout<<ts.sec<<"."<<ts.usec<<"\t";
        }
        cout<<endl; 
    }
}
