#ifndef TRACEGEN_H
#define TRACEGEN_H

#include "stdafx.h"
#include "Address.hpp"
#include "Rule.hpp"
#include "RuleList.h"
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/copy.hpp>
#include <cassert>
#include <thread>
#include <future>
#include <mutex>
#include <atomic>
#include <chrono>
#include <set>
#include <map>

#include <pcap.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

using std::vector;
using std::string;
using std::atomic_uint;
using std::atomic_bool;
using std::mutex;
using boost::unordered_map;
using boost::unordered_set;
namespace fs = boost::filesystem;
namespace io = boost::iostreams;

/*
 * Usage:
 *   tracer tGen(rulelist pointer);
 *   tGen.setpara(parameter file);
 *   tGen.hotspot(reference file)
 *   pFlow_pruning_gen (objective synthetic trace directory)
 */

class tracer
{
public:
    double flow_rate;
    string ref_trace_dir_str;
    double cold_prob;
    uint32_t hotspot_no;
    double hotvtime;

private:
    rule_list * rList;
    uint32_t flow_no;
    double duration;
    double data_rate;
    const double offset;
    double terminT;
    atomic_uint total_packet;
    uint32_t mut_scalar[2];
    string hotspot_ref;

    // locality traffic parameter
    string flowInfoFile_str;    // first arr time of each flow
    string hotcandi_str;	// hotspot candi file
    uint32_t scope[4];		// hotspot probing scope
    uint32_t hot_rule_thres;	// lower bound for indentify a hot rule
    uint32_t hot_candi_no;	// number of hot candidate to generate


    // sources
    string trace_root_dir; 	// the root directory to save all traces
    string gen_trace_dir;	// the directory for generating one specific trace
    

public:
    tracer();
    tracer(rule_list *);

    /* parameter settings 
     * 1. set_para (string para_file): setting the trace generation parameter using a parameter file
     * 2. print_setup (): print the current parameter setting
     */
    void set_para(string); 
    void print_setup() const;

    /* toolkit 
     * 1. trace_get_ts(string trace_ts_file): get the timestamp of the first packet of the traces and record as "path \t ts"
     * 2. vector<fs::path> get_proc_files(string): the vector return version of trace_get_ts() 
     * 3. uint32_t count_proc(): counts the no. of processors in this machine
     * 4. merge_files(string gen_trace_dir): merge the file with format "/ptrace-" and put them into the "gen_trace_dir"
     * 5. hotspot_prob: probing the hotspot
     * 6. hotspot_prob_b: probing the hotspot with a reference file. bool specify whether to mutate the hot area
     * 7. vector<b_rule> gen_seed_hotspot(size_t prepair_no, size_t max_rule): generate seed hotspot for evolving
     * 8. vector<b_rule> evolve_patter(const vector<b_rule> & seed): evolve the seed and generate new hotspots
     * 8. take_snapshot(...): this takes a snapshot (file, start_time, interval, sample_time, whether_do_rule_check) 
     */
    void trace_get_ts(string);
    vector<fs::path> get_proc_files(string) const;
    friend uint32_t count_proc();
    void merge_files(string) const;
    void hotspot_prob(string);
    void hotspot_prob_b(bool = false);
    vector<b_rule> gen_seed_hotspot(size_t, size_t);
    vector<b_rule> evolve_pattern(const vector<b_rule> &);
    void take_snapshot(string, double, double, int, bool = false);

    /* trace generation and evaluation
     * 1. pFlow_pruning_gen(string trace_root_dir): generate traces to the root directory with "Trace_Generate" sub-dir
     * 2. flow_pruning_gen(string trace_dir): generate a specific trace with specific parameter
     * 3. f_pg_st(...): a single thread for mapping and generate traces
     * 4. flow_arr_mp(): obtain the start time of each flow for later use.
     * 5. f_arr_st(...): a single thread for counting the no. of packets for each flow
     * 6. packet_count_mp(...): count the packet of each flow...  // deprecated
     * 7. p_count_st(...): single thread method for packet_count... // deprecated
     * 8. parse_pack_file_mp(string): process the file from pcap directory and process them into 5tup file
     * 9.p_pf_st(vector<string>): obtain the pcap file in vector<string> and do it.
     */
    void pFlow_pruning_gen();
    void flow_pruneGen_mp(unordered_set<addr_5tup> &) const;
    void f_pg_st (fs::path, uint32_t, boost::unordered_map<addr_5tup, std::pair<uint32_t, addr_5tup> > *) const;
    boost::unordered_set<addr_5tup> flow_arr_mp(string) const;
    boost::unordered_set<addr_5tup> f_arr_st (fs::path) const;
    void packet_count_mp(string, string);
    void p_count_st(fs::path, atomic_uint*, mutex *, boost::unordered_map<addr_5tup, uint32_t>*, atomic_bool *);
    void parse_pack_file_mp(string, string, size_t, size_t) const;
    void p_pf_st(vector<string>, size_t) const;
};


// pcap related
#define ETHER_ADDR_LEN	6
#define ETHER_TYPE_IP		(0x0800)
#define ETHER_TYPE_8021Q 	(0x8100)

/* Ethernet header */
struct sniff_ethernet {
    u_char ether_dhost[ETHER_ADDR_LEN]; /* Destination host address */
    u_char ether_shost[ETHER_ADDR_LEN]; /* Source host address */
    u_short ether_type; /* IP? ARP? RARP? etc */
};


/* IP header */
struct sniff_ip {
    u_char ip_vhl;		/* version << 4 | header length >> 2 */
    u_char ip_tos;		/* type of service */
    u_short ip_len;		/* total length */
    u_short ip_id;		/* identification */
    u_short ip_off;		/* fragment offset field */
#define IP_RF 0x8000		/* reserved fragment flag */
#define IP_DF 0x4000		/* dont fragment flag */
#define IP_MF 0x2000		/* more fragments flag */
#define IP_OFFMASK 0x1fff	/* mask for fragmenting bits */
    u_char ip_ttl;		/* time to live */
    u_char ip_p;		/* protocol */
    u_short ip_sum;		/* checksum */
    struct in_addr ip_src,ip_dst; /* source and dest address */
};
#define IP_HL(ip)		(((ip)->ip_vhl) & 0x0f)
#define IP_V(ip)		(((ip)->ip_vhl) >> 4)

/* TCP header */
typedef u_int tcp_seq;

struct sniff_tcp {
    u_short th_sport;	/* source port */
    u_short th_dport;	/* destination port */
    tcp_seq th_seq;		/* sequence number */
    tcp_seq th_ack;		/* acknowledgement number */
    u_char th_offx2;	/* data offset, rsvd */
#define TH_OFF(th)	(((th)->th_offx2 & 0xf0) >> 4)
    u_char th_flags;
#define TH_FIN 0x01
#define TH_SYN 0x02
#define TH_RST 0x04
#define TH_PUSH 0x08
#define TH_ACK 0x10
#define TH_URG 0x20
#define TH_ECE 0x40
#define TH_CWR 0x80
#define TH_FLAGS (TH_FIN|TH_SYN|TH_RST|TH_ACK|TH_URG|TH_ECE|TH_CWR)
    u_short th_win;		/* window */
    u_short th_sum;		/* checksum */
    u_short th_urp;		/* urgent pointer */
};

#endif
