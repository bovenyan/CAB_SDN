#include "stdafx.h"
#include "Address.hpp"
#include "Rule.hpp"
#include "RuleList.h"
#include "BucketTree.h"
#include "OFswitch.h"
#include "TraceGen.h"
#include "TraceAnalyze.h"

namespace fs = boost::filesystem;

int main(int argc, char* argv [])
{

    //trace_plot_hp("./hdr-trace.gz", "./hdr-plot.dat");

    /*
    string rulefile = "../../classbench/rulesets/rule4000";
    rule_list rList(rulefile);
    rList.obtain_dep();

    bucket_tree bTree(rList, size_t(20));
    bTree.pre_alloc();
    OFswitch ofswitch;
    ofswitch.set_para("./mode_file", &rList, &bTree);
    ofswitch.run_test();
    */

    srand(time(NULL));
    string rulefile = "./para_src/rule4000";
    rule_list rList(rulefile);
    rList.obtain_dep();

    tracer Tgen(&rList);
    Tgen.set_para("./para_src/para_file.txt");


    /*
    for (int i = 1; i<argc; ++i){
    	if (!strcmp(argv[i], "-flow_rate"))
    		Tgen.flow_rate = boost::lexical_cast<double>(argv[i+1]);
    	if (!strcmp(argv[i],"-hot_spot"))
    		Tgen.hotspot_no = boost::lexical_cast<size_t>(argv[i+1]);
    	if (!strcmp(argv[i],"-cold_porb"))
    		Tgen.cold_prob = boost::lexical_cast<double>(argv[i+1]);
    	if (!strcmp(argv[i],"-hot_arr"))
    		Tgen.hotvtime = boost::lexical_cast<double>(argv[i+1]);
    }*/
    // generate trace
    for (int i = 5; i<21; ++i)
    {
        cout<<"GENerating.. file "<<i <<endl;
        Tgen.flow_rate = 10*i;
        //Tgen.cold_prob = 0.00125*i;
        Tgen.cold_prob = 0.0005*i+0.005;
        Tgen.hotspot_no = 10*i;
        Tgen.print_setup();
        Tgen.pFlow_pruning_gen(".");
    }


    /*
    bucket_tree bTree(rList, size_t(20));
    bTree.pre_alloc();
    OFswitch ofswitch;
    ofswitch.set_para("./para_src/mode_file", &rList, &bTree);
    ofswitch.tracefile_str = "./ref_trace.gz";
    ofswitch.TCAMcap = 1500;
    ofswitch.mode = 0;
    ofswitch.run_test();
    */

    /*
    fs::directory_iterator end;
    fs::path dir("./Trace_Generate/");

    for (fs::directory_iterator iter(dir); (iter!= end); ++iter){
    	//cout << "CAB: " << endl;
    	//ofswitch.tracefile_str = iter->path().string()+"/GENtrace/ref_trace.gz";
    	//ofswitch.mode = 0;
    	//ofswitch.TCAMcap = 1500;
    	//ofswitch.run_test();
    	ofswitch.tracefile_str = iter->path().string()+"/IDtrace/ref_trace.gz";
    	cout << "CEM " << " ===== "<<ofswitch.tracefile_str<<endl;
    	ofswitch.mode = 1;
    	ofswitch.TCAMcap = 100000;
    	ofswitch.run_test();
    	cout << endl;
    }*/
}
