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

void print_usage() {
    std::cerr << "Usage: TracePrepare_blk <TracePrepare_blk_config.ini> [rules_file] [tree_preallocate_data] [para_file_blk] [trace_root_dir]"
              << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string rulesFileName;
    std::string treePreallocateDataFileName;
    std::string paraFileBlkFileName;
    std::string traceRootDir;
    int iThresholdHard;
    bool bIsHotspot_prob_b_mutation;

    std::ifstream iniFile(argv[1]);
    std::string strLine;
    std::vector<string> vector_parameters;

    for (; std::getline(iniFile, strLine); ) {
        vector_parameters = std::vector<string>();
        boost::split(vector_parameters, strLine, boost::is_any_of(" \t"));
        if ("rules_file" == vector_parameters[0]) {
            rulesFileName = vector_parameters[1];
        }
        if ("tree_preallocate_data" == vector_parameters[0]) {
            treePreallocateDataFileName = vector_parameters[1];
        }
        if ("para_file_blk" == vector_parameters[0]) {
            paraFileBlkFileName = vector_parameters[1];
        }
        if ("trace_root_dir" == vector_parameters[0]) {
            traceRootDir = vector_parameters[1];
        }
        if ("threshold_hard" == vector_parameters[0]) {
            iThresholdHard = std::stoi(vector_parameters[1]);
        }
        if ("hotspot_prob_b_mutation" == vector_parameters[0]) {
            if ("true" == vector_parameters[1]) bIsHotspot_prob_b_mutation = true;
            else bIsHotspot_prob_b_mutation = false;
        }
    }

    if (argc > 2) {
        rulesFileName = std::string(argv[2]);
    }
    if (argc > 3) {
        treePreallocateDataFileName = std::string(argv[3]);
    }
    if (argc > 4) {
        paraFileBlkFileName = std::string(argv[4]);
    }
    if (argc > 5) {
        traceRootDir = std::string(argv[5]);
    }


    // init log, rule list, randomness
    srand (time(NULL));
    logging_init();
    rule_list rList(rulesFileName);

    // generate bucket tree
    bucket_tree bTree(rList, iThresholdHard);
    bTree.pre_alloc();
    bTree.print_tree(treePreallocateDataFileName);

    // trace generation
    tracer tGen(&rList);
    tGen.set_para(paraFileBlkFileName);
    tGen.hotspot_prob_b(bIsHotspot_prob_b_mutation);

    tGen.trace_root_dir = traceRootDir;

    for (int i = 1; i < 20; ++i) {
        tGen.flow_rate = 10*i;
        tGen.hotspot_no = 50+2*i;
        tGen.cold_prob = 0.001+0.0001*i;
        tGen.hotvtime = 20;
        tGen.pFlow_pruning_gen(false);
    }

    //tGen.raw_snapshot("./Packet_File/sample-10-12", 10, 300);
    //tGen.raw_hp_similarity("./Packet_File/sample-10-12", 3600, 30, 120, 20);
    //tGen.parse_pcap_file_mp(557,594);
}
