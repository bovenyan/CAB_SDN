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

/* initalize log  */
void logging_init() {
    fs::create_directory("./log");
    logging::add_file_log
    (
        keywords::file_name = "./log/sample_%N.log",
        keywords::rotation_size = 10 * 1024 * 1024,
        keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0),
        keywords::format = "[%TimeStamp%]: %Message%"
    );

    /* set severity  */
    /* logging::core::get()->set_filter
     * (
     *     logging::trivial::severity >= warning
     * ); */
}

void print_usage() {
    std::cerr << "Usage: CABDeamon <CABDaemon_config.ini> [rules_file] [tree_preallocate_data] [para_file]"
              << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    srand (time(NULL));
    logging_init();

    std::string rulesFileName;
    std::string treePreallocateFileName;
    std::string paraFileName;
    bool bIs2tup;
    int iThresholdHard;
    bool bIs_hotspot_prob_b_mutation;
    bool bIs_pFlow_pruning_gen_evolving;

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
            treePreallocateFileName = vector_parameters[1];
        }
        if ("para_file" == vector_parameters[0]) {
            paraFileName = vector_parameters[1];
        }
        if ("threshold_hard" == vector_parameters[0]) {
            iThresholdHard = std::stoi(vector_parameters[1]);
        }
        if ("2tup_or_4tup" == vector_parameters[0]) {
            if ("true" == vector_parameters[1]) bIs2tup = true;
            else bIs2tup = false;
        }
        if ("hotspot_prob_b_mutation" == vector_parameters[0]) {
            if ("true" == vector_parameters[1]) bIs_hotspot_prob_b_mutation = true;
            else bIs_hotspot_prob_b_mutation = false;
        }
        if ("pFlow_pruning_gen_evolving" == vector_parameters[0]) {
            if ("true" == vector_parameters[1]) bIs_pFlow_pruning_gen_evolving = true;
            else bIs_pFlow_pruning_gen_evolving = false;
        }
    }

    if (argc > 2) {
        rulesFileName = std::string(argv[2]);
    }
    if (argc > 3) {
        treePreallocateFileName = std::string(argv[3]);
    }
    if (argc > 4) {
        paraFileName = std::string(argv[4]);
    }

    /* apply true for testbed, 2 tuple rule */
    rule_list rList(rulesFileName, bIs2tup);

    // rList.print("../para_src/rList.dat");

    // generate bucket tree
    bucket_tree bTree(rList, iThresholdHard, bIs2tup);
    // bTree.pre_alloc();
    bTree.print_tree(treePreallocateFileName);

    /* trace generation */
    tracer tGen(&rList);
    tGen.set_para(paraFileName);
    tGen.hotspot_prob_b(bIs_hotspot_prob_b_mutation);
    tGen.pFlow_pruning_gen(bIs_pFlow_pruning_gen_evolving);

    //tGen.raw_snapshot("./Packet_File/sample-10-12", 10, 300);
    //tGen.raw_hp_similarity("./Packet_File/sample-10-12", 3600, 30, 120, 20);
    //tGen.parse_pcap_file_mp(557,594);
}
