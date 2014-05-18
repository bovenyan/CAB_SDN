#include "stdafx.h"
#include "Rule.hpp"
#include "RuleList.h"
#include "BucketTree.h"
#include "TraceGen.h"
#include "OFswitch.h"

using std::cout;
using std::endl;

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace fs = boost::filesystem;

using namespace logging::trivial;

void logging_init() {
    fs::create_directory("./log");
    logging::add_file_log
    (
        keywords::file_name = "./log/sample_%N.log",
        keywords::rotation_size = 10 * 1024 * 1024,
        keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0),
        keywords::format = "[%TimeStamp%]: %Message%"
    );

    logging::core::get()->set_filter
    (
        logging::trivial::severity >= warning
    );
}

int main() {
    // init log, rule list
    logging_init();
    string rulefile = "../para_src/rule4000";
    rule_list rList(rulefile);
    
    // generate bucket tree
    bucket_tree bTree(rList, 15);
    //bTree.print_tree("../para_src/tree.dat", false);
    bTree.pre_alloc();
    bTree.print_tree("../para_src/tree_pr.dat", false);

    // trace generation
    tracer tGen(&rList);
    tGen.set_para("../para_src/para_file.txt");
    tGen.hotspot_prob_b("../para_src/tree_pr.dat", false);
    tGen.pFlow_pruning_gen("..");
    
    // unit test: buck_tree generation and search
    bTree.search_test("../Trace_Generate/trace-20k-0.01-10/GENtrace/ref_trace.gz");

    // unit test: static test
    /*
    bTree.static_traf_test("../para_src/hotspot.dat_m");
    */

    /*
    tGen.pFlow_pruning_gen("..");
    */

    // unit test: bucket split
    /*
    //string bucket_str = "122.2.116.36/31\t53.26.7.64/27\t0.0.0.0/16\t0.0.16.0/20";
    string bucket_str = "0.0.0.0/0\t0.0.0.0/0\t0.0.0.0/16\t0.0.0.0/16";
    bucket buck(bucket_str, &rList);
    cout<<buck.get_str()<<endl;
    vector<size_t> cut1;
    cut1.push_back(1), cut1.push_back(0), cut1.push_back(0), cut1.push_back(0);
    auto result = buck.split(cut1, &rList);
    cout<<"result "<< result.first<<"\t"<<result.second <<endl;
    if (result.first > 0){
    	for (auto iter = buck.sonList.begin(); iter != buck.sonList.end(); ++iter)
    		cout<<(*iter)->get_str()<<endl;
    }
    cut1.clear();
    cut1.push_back(0), cut1.push_back(1), cut1.push_back(0), cut1.push_back(0);
    result = buck.split(cut1, &rList);
    cout<<"result "<< result.first<<"\t"<<result.second <<endl;
    if (result.first > 0){
    	for (auto iter = buck.sonList.begin(); iter != buck.sonList.end(); ++iter)
    		cout<<(*iter)->get_str()<<endl;
    }

    cout << "related rules" << endl;
    for (auto iter = buck.related_rules.begin(); iter != buck.related_rules.end(); ++iter)
    	rList.list[*iter].print();
    */

    return 0;
}



