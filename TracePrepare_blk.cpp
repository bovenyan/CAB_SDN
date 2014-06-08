#include "stdafx.h"
#include "Rule.hpp"
#include "RuleList.h"
#include "BucketTree.h"
#include "TraceGen.h"


using std::cout;
using std::endl;
using std::ofstream;

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;
namespace fs = boost::filesystem;

void logging_init() {
    fs::create_directory("./log");
    logging::add_file_log
    (
        keywords::file_name = "./log/sample_%N.log",
        keywords::rotation_size = 10 * 1024 * 1024,
        keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0),
        keywords::format = "[%TimeStamp%]: %Message%"
    );

    /*logging::core::get()->set_filter
    (
        logging::trivial::severity >= warning
    );*/
}

int main() {
    // init log, rule list, randomness
    srand (time(NULL));
    logging_init();
    string rulefile = "../para_src/ruleset/acl_8000";
    rule_list rList(rulefile);
    rList.print("../para_src/rList.dat");
    
    // generate bucket tree
    bucket_tree bTree(rList, 15);
    bTree.print_tree("../para_src/tree_pr.dat");

    // trace generation
    tracer tGen(&rList);
    tGen.set_para("../para_src/para_file_blk.txt");
    tGen.hotspot_prob_b(false);

    tGen.flow_rate = 250;
    tGen.hotspot_no = 80;
    tGen.hotvtime = 10;
    tGen.pFlow_pruning_gen(false);
    /*
    for (int i = 1; i < 7; ++i){
	tGen.flow_rate = 50*i;
	tGen.hotspot_no = 50+5*i;
	tGen.hotvtime = 20;
    	tGen.pFlow_pruning_gen(false);
    }
    */

    //tGen.raw_snapshot("./Packet_File/sample-10-12", 10, 300);
    //tGen.raw_hp_similarity("./Packet_File/sample-10-12", 3600, 30, 120, 20);
    //tGen.parse_pcap_file_mp(557,594);
}
