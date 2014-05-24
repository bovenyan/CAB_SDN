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
    string flowInfoFile_str;
    string hotcandi_str;
    uint32_t scope[4];
    uint32_t hot_rule_thres;
    uint32_t hot_candi_no;

public:
    tracer();
    tracer(rule_list *);

    // parameter setting and preparation
    void set_para(string);
    void print_setup() const;

    void trace_get_ts(string);
    vector<fs::path> get_proc_files(string) const;
    friend uint32_t count_proc();
    void merge_files(string) const;

    void hotspot_prob(string);
    void hotspot_prob_b(bool = false);

    // trace generation and evaluation
    void pFlow_pruning_gen(string);
    void flow_pruneGen_mp(unordered_set<addr_5tup> &, fs::path) const;
    void f_pg_st (fs::path, uint32_t, string, boost::unordered_map<addr_5tup, std::pair<uint32_t, addr_5tup> > *) const;
    boost::unordered_set<addr_5tup> flow_arr_mp(string) const;
    boost::unordered_set<addr_5tup> f_arr_st (fs::path) const;
    void packet_count_mp(string, string);
    void p_count_st(fs::path, atomic_uint*, mutex *, boost::unordered_map<addr_5tup, uint32_t>*, atomic_bool *);
    void printTestTrace(string);
};


#endif
