#include <arpa/inet.h>
#include <cstring>
#include <ctime>
#include <pcap.h>
#include <fstream>
#include <string>
#include <set>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/filesystem.hpp>
#include "PcapHeader.hpp"
#include "TimeSpec.hpp"
#include "Address.hpp"

#define MAX_ETHER_FRAME_LEN 1514
#define READ_TIMEOUT 1000

using namespace std;
namespace fs = boost::filesystem;
namespace io = boost::iostreams;

// std::set<string> flows;

// string str_readable(const addr_5tup & h){
//     stringstream ss;
//     for (uint32_t i = 0; i < 2; i++) {
//         for (uint32_t j = 0; j < 4; j++) {
//             ss << ((h.addrs[i] >> (24-8*j)) & ((1<<8)-1));
//             if (j!=3)
//                 ss<<".";
//         }
//         ss<<"\t";
//     }
//     for (uint32_t i = 2; i < 4; i++)
//         ss<<h.addrs[i]<<"\t";

//     return ss.str();
// }

int make_pkt(const addr_5tup & header, uint8_t ** data, uint32_t * pkt_len) {

    uint32_t payload_size = sizeof(timespec);
    uint32_t buffer_size = sizeof(sniff_ethernet) + sizeof(sniff_ip) + 
                           sizeof(sniff_tcp) + payload_size;
    uint8_t * buffer = new uint8_t[buffer_size];

    memset(buffer,0,buffer_size);

    sniff_ethernet * eth = (sniff_ethernet *)buffer;
    sniff_ip * ip = (sniff_ip *)(buffer+sizeof(sniff_ethernet));
    sniff_tcp * tcp = (sniff_tcp *)(buffer + sizeof(sniff_ethernet) +
                                    sizeof(sniff_ip));
    uint8_t * body = buffer + sizeof(sniff_ethernet) +
                     sizeof(sniff_ip) + sizeof(sniff_tcp);

    /* map TCP port into mac for wildcard testing */
    *eth = sniff_ethernet();
    uint32_t src_port = htonl(header.addrs[2]);
    uint32_t dst_port = htonl(header.addrs[3]);
    memcpy(eth->ether_shost + 2, &src_port, 4);
    memcpy(eth->ether_dhost + 2, &dst_port, 4);

    /* IP source  */
    *ip = sniff_ip();
    *tcp = sniff_tcp();
    ip->ip_src.s_addr = htonl(header.addrs[0]);
    ip->ip_dst.s_addr = htonl(header.addrs[1]);
    ip->ip_len = htonl(buffer_size - sizeof(sniff_ethernet));

    /* make time stamp */
    timespec * timestamp = (timespec *)body;
    clock_gettime(CLOCK_REALTIME,timestamp);

    *data = buffer;
    *pkt_len = buffer_size;

    return 0;
}

void print_help() {
    cerr << "Usage: FlowGen {trace_file} {-i interface |-f pcap_file} factor";
    cerr << endl;
}

int main(int argc, char * argv[]) {
    /* parse arguments  */
    const char * trace_file_str = NULL;
    const char * if_name = NULL;
    bool flow_gen_flag = true;

    pcap_t * pd = nullptr;
    char pebuf[PCAP_ERRBUF_SIZE];

    int factor = 1;

    if (argc < 4) {
        print_help();
        return 1;
    }


    trace_file_str = argv[1];

    if (strcmp("-i", argv[2]) == 0) {
        /* this is for sending packets */
        pd = pcap_open_live(argv[3], MAX_ETHER_FRAME_LEN, 1,
                            READ_TIMEOUT, pebuf);
    } else if (strcmp("-f",argv[2] ) == 0) {
        pd = pcap_open_dead(DLT_EN10MB, 144);
        pcap_dumper_t * pfile = pcap_dump_open(pd,argv[3]);
    } else {
        cerr << "Wrong input" << endl;
        print_help();
        return 3;
    }

    if (argc == 5) {
        factor = atoi(argv[4]);
    }

    /* start sending  */
    ifstream trace_file(trace_file_str);

    if (!trace_file.is_open()) {
        cerr << "Can not open trace file : " << trace_file_str << endl;
        return 2;
    }


    try {
        io::filtering_istream in;
        in.push(io::gzip_decompressor());
        in.push(trace_file);
        string line;
        TimeSpec zero, now;

        /* set birth time */
        clock_gettime(CLOCK_MONOTONIC, &zero.time_point_);

        /* for recording packet sent */
        // std::ofstream ofs;
        // ofs.open("./flows", std::ofstream::out);
        while(getline(in,line)) {
            addr_5tup pkt_header(line);

            uint8_t * pkt = nullptr;
            uint32_t  pkt_len = 0;

            /* set packet interval */
            TimeSpec next_pkt_ts(pkt_header.timestamp * factor);
            clock_gettime(CLOCK_MONOTONIC, &now.time_point_);

            if (now < zero + next_pkt_ts) {
                TimeSpec to_sleep = next_pkt_ts + zero - now;
                nanosleep(&to_sleep.time_point_, nullptr);
            }

            // string ph_str (str_readable(pkt_header));
            // if (flows.find(ph_str) == flows.end())
            // {
            //     flows.insert(ph_str);
            //     ofs << ph_str << endl;
            // }

            make_pkt(pkt_header,&pkt,&pkt_len);
            pcap_sendpacket(pd,pkt,pkt_len);

            delete [] pkt;
        }
        // ofs.close();
    } catch(std::exception & e) {
        cerr << e.what() << endl;
    }
}
