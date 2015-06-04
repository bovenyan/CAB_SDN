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
    string rulefile = "../para_src/ruleset/8k_4";
    rule_list rList(rulefile, false);
    rList.print("../para_src/rList.dat");
    
    // generate bucket tree
    bucket_tree bTree(rList, 20, false);
//    bTree.pre_alloc();
    bTree.print_tree("../para_src/tree_pr.dat");

    // trace generation
    tracer tGen(&rList);
    tGen.set_para("../para_src/trace_para.txt");
    tGen.hotspot_prob_b(false);
    for (int fr = 5; fr <= 50; fr += 5){
        tGen.flow_rate = fr;
        tGen.pFlow_pruning_gen(false);
    }

}
