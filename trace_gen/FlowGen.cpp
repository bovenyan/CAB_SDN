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
#include <getopt.h>

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

    memset(buffer, 0, buffer_size);

    sniff_ethernet * eth = (sniff_ethernet *)buffer;
    sniff_ip * ip = (sniff_ip *)(buffer+sizeof(sniff_ethernet));
    sniff_tcp * tcp = (sniff_tcp *)(buffer + sizeof(sniff_ethernet) +
                                    sizeof(sniff_ip));
    uint8_t * body = buffer + sizeof(sniff_ethernet) +
                     sizeof(sniff_ip) + sizeof(sniff_tcp);

    /* IPv4 4 tuple mapping -> TCP port to MAC  */
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

int make_pkt_ipv6(const addr_5tup & header, uint8_t ** data, uint32_t * pkt_len) {
    uint32_t payload_size = sizeof(timespec);
    uint32_t buffer_size = sizeof(sniff_ethernet) + sizeof(sniff_ipv6) +
                           sizeof(sniff_tcp) + payload_size;
    uint8_t * buffer = new uint8_t[buffer_size];

    memset(buffer, 0, buffer_size);

    sniff_ethernet * eth = (sniff_ethernet *)buffer;
    sniff_ipv6 * ip = (sniff_ipv6 *)(buffer+sizeof(sniff_ethernet));
    sniff_tcp * tcp = (sniff_tcp *)(buffer + sizeof(sniff_ethernet) +
                                    sizeof(sniff_ipv6));
    uint8_t * body = buffer + sizeof(sniff_ethernet) +
                     sizeof(sniff_ipv6) + sizeof(sniff_tcp);

    //DEBUG
    std::cout<<"sizeOfIPv6 = "<<sizeof(sniff_ipv6)<<endl;
    std::cout<<"before sniff_ethernet()\n";

    *eth = sniff_ethernet();

    /* Map ipv4:port to ipv6  */
    // TODO: check big edian/small edian
    std::cout<<"before map ipv4+port to ipv6"<<endl;
    *ip = sniff_ipv6();
    *tcp = sniff_tcp();
    *(uint32_t *)(ip->ip_src.s6_addr + 12) = htonl(header.addrs[0]);
    *(uint32_t *)(ip->ip_src.s6_addr + 4) = htonl(header.addrs[2]);
    *(uint32_t *)(ip->ip_dst.s6_addr + 12) = htonl(header.addrs[1]);
    *(uint32_t *)(ip->ip_dst.s6_addr + 4) = htonl(header.addrs[3]);
    ip->ip_len = htonl(buffer_size - sizeof(sniff_ethernet));

    //DEBUG
    std::cout<<"before making time stamp\n";
    /* make time stamp */
    timespec * timestamp = (timespec *)body;
    clock_gettime(CLOCK_REALTIME, timestamp);

    *data = buffer;
    *pkt_len = buffer_size;

    //DEBUG
    std::cout<<"before return of mk_pkt_ipv6"<<endl;
    return 0;
}

void print_help() {
    cerr << "Usage: FlowGen {-s stats_file -i interface -f pcap_file -F factor -ipv6/ipv4}";
    cerr << endl;
}

int main(int argc, char * argv[]) {
    /* configuration  */
    char trace_file_str[100];
    char if_name[10];
    char stat_file_str[100];
    int factor = 1;
    int ipv6_flag = 1;

    pcap_t * pd = nullptr;
    char pebuf[PCAP_ERRBUF_SIZE];

    int getopt_res;
    while (1) {
        static struct option tracegen_options[] = {
            {"ipv6",        no_argument,                &ipv6_flag, 1},
            {"ipv4",        no_argument,                &ipv6_flag, 0},
            {"help",        no_argument,                0, 'h'},
            {"file",        required_argument,          0, 'f'},
            {"interface",   required_argument,          0, 'i'},
            {"stats",       required_argument,          0, 's'},
            {"scale",       required_argument,          0, 'S'},
            {0,             0,                          0,  0}
        };

        int option_index = 0;

        getopt_res = getopt_long (argc, argv, "hf:i:s:F:",
                                  tracegen_options, &option_index);

        if (getopt_res == -1)
            break;

        switch (getopt_res) {
        case 0:
            if (tracegen_options[option_index].flag != 0)
                break;
        case 'f':
            strcpy(trace_file_str, optarg);
            break;
        case 'i':
            strcpy(if_name, optarg);
            break;
        case 's':
            strcpy(stat_file_str, optarg);
            break;
        case 'F':
            factor = atoi(optarg);
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

    if (if_name == NULL || trace_file_str == NULL || stat_file_str == NULL) {
        print_help();
        return 0;
    }

    // printf("%s, %s, %s, %i, %i\n", trace_file_str, if_name, stat_file_str, ipv6_flag, factor);
    // return 0;

    pd = pcap_open_live(if_name, MAX_ETHER_FRAME_LEN, 1,
                        READ_TIMEOUT, pebuf);

    //DEBUG
    std::cout<<"before sending"<<endl;

    /* start sending  */
    ifstream trace_file(trace_file_str);

    if (!trace_file.is_open()) {
        cerr << "Can not open trace file : " << trace_file_str << endl;
        print_help();
        return 2;
    }


    //DEBUG
    std::cout<<"before try"<<endl;

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
        std::cout<<"before sending 2 flows"<<endl;
        //for debugging use, send 2 flows.
        int iTest2FLow = 2;
        while(getline(in,line)&&iTest2FLow) {
            --iTest2FLow;
            addr_5tup pkt_header(line);

            uint8_t * pkt = nullptr;
            uint32_t  pkt_len = 0;
            //DEBUG
            std::cout<<"before setting packet interval"<<endl;
            /* set packet interval */
            TimeSpec next_pkt_ts(pkt_header.timestamp * factor);
            clock_gettime(CLOCK_MONOTONIC, &now.time_point_);
            //DEBUG
            std::cout<<"before if expression"<<endl;
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
            //DEBUG
            std::cout<<"before making ip packet\n";
            if (ipv6_flag) {
                make_pkt_ipv6(pkt_header,&pkt,&pkt_len);
            } else {
                make_pkt(pkt_header,&pkt,&pkt_len);
            }
            std::cout<<"packet length == "<<pkt_len<<endl;
            std::cout<<"the return of pcap_sendpacket == "<<pcap_sendpacket(pd,pkt,pkt_len)<<endl;

            delete [] pkt;
        }
        // ofs.close();
    } catch(std::exception & e) {
        cerr << e.what() << endl;
    }
}
