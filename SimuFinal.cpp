#include "stdafx.h"
#include "Address.hpp"
#include "Rule.hpp"
#include "RuleList.h"
#include "BucketTree.h"
#include "OFswitch.h"
#include "TraceGen.h"
#include "TraceAnalyze.h"

namespace fs = boost::filesystem;

int main(int argc, char* argv []) {
    srand(time(NULL));
    string rulefile = "./para_src/rule4000";
    rule_list rList(rulefile);
    rList.obtain_dep();

    tracer Tgen(&rList);
    Tgen.set_para("./para_src/para_file.txt");

}
