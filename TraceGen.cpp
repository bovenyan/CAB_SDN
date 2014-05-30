#include "TraceGen.h"

using std::string;
using std::vector;
using std::pair;
using std::list;
using std::set;
using std::cout;
using std::endl;
using std::ifstream;
using std::ofstream;
using std::stringstream;
using std::thread;
using std::atomic_uint;
using std::atomic_bool;
using std::mutex;
using boost::unordered_map;
using boost::unordered_set;
namespace fs = boost::filesystem;
namespace io = boost::iostreams;

typedef boost::unordered_map<addr_5tup, uint32_t>::iterator Map_iter;

/* tracer
 *
 * function brief:
 * constructor: set rule list and simulation time offset
 */
tracer::tracer():offset(346.844), total_packet(0) {
    rList = NULL;
    flow_no = 0;
}

tracer::tracer(rule_list * rL):offset(346.844), total_packet(0) {
    rList = rL;
    flow_no = 0;
}


// ===================== para setting ======================================

/* set_para
 *
 * input: string loc_para_str: path string of parameter file
 *
 * function brief:
 * set the parameters according to a parameter file
 */
void tracer::set_para(string loc_para_str) {
    ifstream ff(loc_para_str);
    for (string str; getline(ff, str); ) {
        vector<string> temp;
        boost::split(temp, str, boost::is_any_of("\t"));
        if (!temp[0].compare("ref_t")) {
            ref_trace_dir_str = temp[1];
        }
        if (!temp[0].compare("hot_c")) {
            hotcandi_str = temp[1];
        }
        if (!temp[0].compare("p_arr")) {
            data_rate = boost::lexical_cast<double>(temp[1]);
        }
        if (!temp[0].compare("f_arr")) {
            flow_rate = boost::lexical_cast<double>(temp[1]);
        }
        if (!temp[0].compare("simu_T")) {
            duration = boost::lexical_cast<double>(temp[1]);
            terminT = duration+offset;
        }
        if (!temp[0].compare("hot_no")) {
            hotspot_no = boost::lexical_cast<uint32_t>(temp[1]);
        }
        if (!temp[0].compare("hot_arr")) {
            hotvtime = boost::lexical_cast<double>(temp[1]);
        }
        if (!temp[0].compare("c_prob")) {
            cold_prob = boost::lexical_cast<double>(temp[1]);
        }
        if (!temp[0].compare("hr_thres")) {
            hot_rule_thres = boost::lexical_cast<uint32_t>(temp[1]);
        }
        if (!temp[0].compare("h_candi_no")) {
            hot_candi_no = boost::lexical_cast<uint32_t>(temp[1]);
        }
        if (!temp[0].compare("he_count")) {
            flowInfoFile_str = temp[1];
        }
        if (!temp[0].compare("hot_ref")) {
            hotspot_ref = temp[1];
        }
        if (!temp[0].compare("mutate_scalar")) {
            vector<string> temp1;
            boost::split(temp1, temp[1], boost::is_any_of(" "));
            mut_scalar[0] = boost::lexical_cast<uint32_t>(temp1[0]);
            mut_scalar[1] = boost::lexical_cast<uint32_t>(temp1[1]);
        }
    }
}

void tracer::print_setup() const {
    cout <<" ======= SETUP BRIEF: ======="<<endl;
    cout<<"reference trace directory is in\t"<<ref_trace_dir_str<<endl;
    cout<<"header info is in : " << flowInfoFile_str << endl;
    cout<<"hotspot candidates are in\t"<<hotcandi_str<<endl;
    cout<<"simulation time set as\t"<<duration<<endl;

    cout<<"flow arrival rate set as\t"<<flow_rate <<endl;
    cout<<"packet arrival rate set as\t"<<data_rate <<endl;
    cout<<"hot spots no set as\t" <<hotspot_no << endl;
    cout<<"hot spots arrival rate\t" <<hotvtime<< endl;
    cout<<"probability of falling cold\t"<< cold_prob << endl;

    cout<<"threshold for a hot rule set as\t" <<hot_rule_thres << endl;
    cout<<"no. of candidate of hotspots\t" <<hot_candi_no << endl;
    cout <<" ======= SETUP DONE: ======="<<endl;
}

/* trace_get_ts
 *
 * input: string trace_ts_file: the timestamp output for each file
 *
 * function brief:
 * 	  outputs the first packet timestamp of each trace file, helps determine how many trace files to use
 */
void tracer::trace_get_ts(string trace_ts_file) {
    fs::path dir(ref_trace_dir_str);
    fs::directory_iterator end;
    ofstream ffo(trace_ts_file);
    if (fs::exists(dir) && fs::is_directory(dir)) {
        for (fs::directory_iterator iter(dir); (iter != end); ++iter) {
            if (fs::is_regular_file(iter->status())) {
                try {
                    io::filtering_istream in;
                    in.push(io::gzip_decompressor());
                    ifstream infile(iter->path().c_str());
                    in.push(infile);
                    string str;
                    getline(in, str);
                    addr_5tup f_packet(str);
                    ffo<<iter->path() << "\t" << f_packet.timestamp <<endl;
                    io::close(in); // careful
                } catch (const io::gzip_error & e) {
                    cout<<e.what()<<std::endl;
                }

            }
        }
    }
    return;
}

/* get_proc_file
 *
 * input: string ref_trace_dir: real trace directory
 * output:vector <fs::ps> :     paths of the real trace files in need for process
 *
 * function_brief:
 * get the paths of real traces within the range of the simulation Time
 */
vector<fs::path> tracer::get_proc_files ( string ref_trace_dir) const {
    // find out how many files to process
    fs::path dir(ref_trace_dir);
    fs::directory_iterator end;
    vector<fs::path> to_proc_files;
    if (fs::exists(dir) && fs::is_directory(dir)) {
        for (fs::directory_iterator iter(dir); iter != end; ++iter) {
            try {
                if (fs::is_regular_file(iter->status())) {
                    io::filtering_istream in;
                    in.push(io::gzip_decompressor());
                    ifstream infile(iter->path().c_str());
                    in.push(infile);
                    string str;
                    getline(in,str);
                    addr_5tup packet(str); // readable
                    if (packet.timestamp > terminT) {
                        io::close(in);
                        break;
                    }
                    to_proc_files.push_back(iter->path());
                    io::close(in);
                }
            } catch(const io::gzip_error & e) {
                cout<<e.what()<<endl;
            }
        }
    }
    return to_proc_files;
}

/* count_proc
 *
 * output: uint32_t: the count no. of processors, helps determine thread no.
 */
uint32_t count_proc() {
    ifstream infile ("/proc/cpuinfo");
    uint32_t counter = 0;
    for (string str; getline(infile,str); )
        if (str.find("processor\t") != string::npos)
            counter++;
    return counter;
}

/* merge_files
 *
 * input: string gen_trace_dir: the targetting dir
 *
 * function_brief:
 * Collects the gz traces with prefix "ptrace-";
 * Merge into one gz trace named "ref_trace.gz"
 */
void tracer::merge_files(string gen_trace_dir) const {
    fs::path file (gen_trace_dir + "/ref_trace.gz");
    if (fs::exists(file))
        fs::remove(file);

    for (uint32_t i = 0; ; ++i) {
        stringstream ss;
        //ss<<gen_trace_dir<<"/ref_trace-"<<i<<".gz";
        ss<<gen_trace_dir<<"/ptrace-"<<i<<".gz";
        fs::path to_merge(ss.str());
        if (fs::exists(to_merge)) {
            io::filtering_ostream out;
            out.push(io::gzip_compressor());
            ofstream out_file(gen_trace_dir+"/ref_trace.gz", std::ios_base::app);
            out.push(out_file);
            cout << "Merging:" << ss.str()<<endl;
            io::filtering_istream in;
            in.push(io::gzip_decompressor());
            ifstream in_file(ss.str().c_str());
            in.push(in_file);
            io::copy(in, out);
            in.pop();
            fs::remove(to_merge);
            out.pop();
        } else
            break;
    }
}

/* hotspot_prob
 *
 * input: string sav_str: output file path
 *
 * function brief:
 * probe and generate the hotspot candidate and put them into a candidate file for later header mapping
 */
void tracer::hotspot_prob(string sav_str) {
    uint32_t hs_count = 0;
    ofstream ff (sav_str);
    uint32_t probe_scope[4];
    uint32_t trial = 0;
    while (hs_count < hot_candi_no) {
        vector<p_rule>::iterator iter = rList->list.begin();
        advance (iter, rand() % rList->list.size());
        // cout<< "candi rule:" << iter->get_str() << endl;
        addr_5tup probe_center = iter->get_random();

        for (uint32_t i=0; i<4; i++)
            probe_scope[i] = scope[i]/2 + rand()%scope[i];
        h_rule hr (probe_center, probe_scope);
        cout << hr.get_str() << "\t" << hr.cal_rela(rList->list) << endl;
        if (hr.cal_rela(rList->list) >= hot_rule_thres) {
            ff << probe_center.str_easy_RW() << "\t" << probe_scope[0] << ":"
               << probe_scope[1] << ":" << probe_scope[2] << ":" <<
               probe_scope[3]<<endl;
            trial = 0;
            ++hs_count;
        }

        ++trial;

        if (trial > 100) {
            cout<<"change para setting"<<endl;
            return;
        }
    }
    ff.close();
}

/* hotspot_prob_b
 *
 * input: string sav_str: output file path
 *
 * function brief:
 * generate the hotspot candidate with some reference file and put them into a candidate file for later header mapping
 */
void tracer::hotspot_prob_b(bool mutation) {
    uint32_t hs_count = 0;
    if (mutation)
        hotcandi_str += "_m";
    ofstream ff (hotcandi_str);
    vector <string> file;
    ifstream in (hotspot_ref);

    cout << "hot_rule_thres " << hot_rule_thres <<endl;
    if (mutation)
        cout <<"mutation scalar " << mut_scalar[0] << " " << mut_scalar[1] << endl;

    for (string str; getline(in, str); ) {
        vector<string> temp;
        boost::split(temp, str, boost::is_any_of("\t"));
        if (boost::lexical_cast<uint32_t>(temp.back()) > hot_rule_thres) {
            if (mutation) {
                b_rule br(str);
                br.mutate_pred(mut_scalar[0], mut_scalar[1]);
                str = br.get_str();
                size_t assoc_no = 0;
                for (auto iter = rList->list.begin(); iter != rList->list.end(); ++iter)
                    if (br.match_rule(*iter))
                        ++assoc_no;
                stringstream ss;
                ss<< str << "\t" << assoc_no;
                str = ss.str();
            }
            file.push_back(str);
        }
    }
    random_shuffle(file.begin(), file.end());
    vector<string>::iterator iter = file.begin();
    while (hs_count < hot_candi_no && iter < file.end()) {
        ff <<*iter<<endl;
        iter++;
        ++hs_count;
    }
    ff.close();
}


vector<b_rule> tracer::gen_seed_hotspot(size_t prepare_no, size_t max_rule) {
    vector<b_rule> gen_traf_block;
    ifstream in (hotspot_ref);
    for (string str; getline(in, str); ) {
        vector<string> temp;
        boost::split(temp, str, boost::is_any_of("\t"));
        if (boost::lexical_cast<uint32_t>(temp.back()) > hot_rule_thres) {
            b_rule br(str);
            size_t assoc_no = 0;
            for (auto iter = rList->list.begin(); iter != rList->list.end(); ++iter) {
                if (br.match_rule(*iter)) {
                    ++assoc_no;
                }
            }
            if (assoc_no > max_rule)
                continue;

            gen_traf_block.push_back(br);
        }
    }
    random_shuffle(gen_traf_block.begin(), gen_traf_block.end());
    if (prepare_no > gen_traf_block.size())
        prepare_no = gen_traf_block.size();
    gen_traf_block.resize(prepare_no);
    return gen_traf_block;
}

vector<b_rule> tracer::evolve_pattern(const vector<b_rule> & seed_hot_spot) {
    vector<b_rule> gen_traf_block;
    for (auto iter = seed_hot_spot.begin(); iter != seed_hot_spot.end(); ++iter) {
        b_rule to_push = *iter;
        to_push.mutate_pred(mut_scalar[0], mut_scalar[1]);
        gen_traf_block.push_back(to_push);
    }
    return gen_traf_block;
}

void tracer::take_snapshot(string tracefile, double start_time, double interval, int sampling_time, bool rule_test) {
    io::filtering_istream ref_trace_stream;
    ref_trace_stream.push(io::gzip_decompressor());
    ifstream ref_trace_file(tracefile);
    ref_trace_stream.push(ref_trace_file);

    set<uint32_t> header_rec;
    vector<boost::unordered_set<addr_5tup> > flow_rec_vec;
    boost::unordered_set<addr_5tup> flow_rec;
    int sample_count = 0;
    double checkpoint = start_time + interval;

    string str;
    getline(ref_trace_stream, str);
    addr_5tup packet(str, false);
    start_time = start_time + packet.timestamp;

    for (string str; getline(ref_trace_stream, str); ) {
        addr_5tup packet(str, false);

	if (packet.timestamp < start_time) // jump through the start 
		continue;
	if (packet.timestamp > checkpoint){ // take next snapsho 
            flow_rec_vec.push_back(flow_rec);
            flow_rec.clear();
	    checkpoint += interval;
        } else {
            header_rec.insert(packet.addrs[0]);
            header_rec.insert(packet.addrs[1]);
            flow_rec.insert(packet);
        }
    }
    io::close(ref_trace_stream);

    set<uint32_t> hitrule_rec;
    for (uint32_t i = 0; i<flow_rec_vec.size(); i++) {
        stringstream ss;
        ss<<"snapshot-"<<i<<"dat";
        ofstream ff (ss.str().c_str());

        for (boost::unordered_set<addr_5tup>::iterator iter=flow_rec_vec[i].begin(); iter != flow_rec_vec[i].end(); iter++) {
            uint32_t src = distance(header_rec.begin(), header_rec.find(iter->addrs[0]));
            uint32_t dst = distance(header_rec.begin(), header_rec.find(iter->addrs[1]));
            ff<<src<<"\t"<<dst<<endl;
	    
	    if (rule_test){ // linear search of rules
            	for (uint32_t j = 0; j < rList->list.size(); j++) {
                	if (rList->list[j].packet_hit(*iter)) {
                    	hitrule_rec.insert(j);
                    	break;
                	}
            	}
	    }
        }
        ff.close();
    }
    
    if (rule_test)
    	cout << hitrule_rec.size() << " rules are hit"<<endl;
}

// ===================================== Trace Generation and Evaluation =========================

/* pFlow_pruning_gen
 *
 * input: string trace_root_dir: target directory
 *
 * function_brief:
 * wrapper function for generate localized traces
 */
void tracer::pFlow_pruning_gen(string trace_root_dir) {
    // get the flow info count
    fs::path dir(trace_root_dir + "/Trace_Generate");
    if (fs::create_directory(dir)) {
        cout<<"creating: " << dir.string()<<endl;
    } else {
        cout<<"exitst: "<<dir.string()<<endl;
    }

    unordered_set<addr_5tup> flowInfo;
    if (flowInfoFile_str != "") {
        ifstream infile(flowInfoFile_str.c_str());
        for (string str; getline(infile, str);) {
            addr_5tup packet(str, false);
            if (packet.timestamp > terminT)
                break;
            flowInfo.insert(packet);
        }
        infile.close();
        cout << "read flow info successful" <<endl;
    } else
        flowInfo = flow_arr_mp("./para_src/flow_info");

    // trace generated in format of  "trace-200k-0.05-20"
    stringstream ss;
    ss<<dir.string()<<"/trace-"<<flow_rate<<"k-"<<cold_prob<<"-"<<hotspot_no;
    fs::path son_dir(ss.str());
    if (fs::create_directory(son_dir)) {
        cout<<"creating: "<<son_dir.string()<<endl;
    } else {
        cout<<"exitst: "<<son_dir.string()<<endl;
    }
    // prune and gen file
    flow_pruneGen_mp(flowInfo, son_dir);
}


/* flow_pruneGen_mp
 * input: unordered_set<addr_5tup> & flowInfo : first packet count
 * 	  string ref_trace_dir : real trace directory
 * 	  string hotspot_candi : candidate hotspot no. generated
 *
 * function_brief:
 * prune the headers according arrival, and map the headers
 */
void tracer::flow_pruneGen_mp( unordered_set<addr_5tup> & flowInfo, fs::path gen_trace_dir) const {
    std::multimap<double, addr_5tup> ts_prune_map;
    for (unordered_set<addr_5tup>::iterator iter=flowInfo.begin(); iter != flowInfo.end(); ++iter) {
        ts_prune_map.insert(std::make_pair(iter->timestamp, *iter));
    }
    cout << "total flow no. : " << ts_prune_map.size() <<endl;

    // prepair hot spots
    list<h_rule> hotspot_queue;
    ifstream in (hotcandi_str);
    for (uint32_t i = 0; i < hotspot_no; i++) {
        string line;
        if (!getline(in, line)) {
            in.clear();
            in.seekg(0, std::ios::beg);
        }
        h_rule hr(line, rList->list);
        hotspot_queue.push_back(hr);
    }

    // smoothing every 10 sec, map the headers
    boost::unordered_map<addr_5tup, pair<uint32_t, addr_5tup> > pruned_map;
    const double smoothing_interval = 10.0;
    double next_checkpoint = offset + smoothing_interval;
    double flow_thres = 10* flow_rate;
    vector< addr_5tup > header_buf;
    header_buf.reserve(3000);
    uint32_t id = 0;
    uint32_t total_header = 0;

    double nextKickOut = offset + hotvtime;

    for (auto iter = ts_prune_map.begin(); iter != ts_prune_map.end(); ++iter) {
        if (iter->first > next_checkpoint) {
            random_shuffle(header_buf.begin(), header_buf.end());
            uint32_t i = 0 ;
            for (i = 0; i < flow_thres && i < header_buf.size(); ++i) {
                addr_5tup header;
                if ((double) rand() /RAND_MAX < (1-cold_prob)) {
                    auto q_iter = hotspot_queue.begin();
                    advance(q_iter, rand()%hotspot_no);
                    header = q_iter->gen_header();
                } else {
                    header = rList->list[(rand()%(rList->list.size()))].get_random();
                }
                pruned_map.insert( std::make_pair(header_buf[i], std::make_pair(id, header)));
                ++id;
            }
            total_header += i;
            header_buf.clear();
            next_checkpoint += smoothing_interval;
        }
        header_buf.push_back(iter->second);

        if (iter->first > nextKickOut) {
            //ff<<hotspot_queue.front().get_str()<<"\t"<<iter->first<<endl;
            hotspot_queue.pop_front();
            string line;
            if (!getline(in, line)) {
                in.clear();
                in.seekg(0, std::ios::beg);
                getline(in, line);
            }
            h_rule hr (line, rList->list);
            hotspot_queue.push_back(hr);
            nextKickOut += hotvtime;
        }

    }
    cout << "after smoothing, average: " << double(total_header)/duration <<endl;

    // process using multi-thread;
    fs::path temp1(gen_trace_dir.string()+"/IDtrace");
    fs::create_directory(temp1);
    fs::path temp2(gen_trace_dir.string()+"/GENtrace");
    fs::create_directory(temp2);


    vector< std::future<void> > results_exp;
    vector< fs::path> to_proc_files = get_proc_files(ref_trace_dir_str);

    for(uint32_t file_id = 0; file_id < to_proc_files.size(); ++file_id) {
        results_exp.push_back(std::async(std::launch::async, &tracer::f_pg_st, this, to_proc_files[file_id], file_id, gen_trace_dir.string(), &pruned_map));
    }

    for (uint32_t file_id = 0; file_id < to_proc_files.size(); ++file_id) {
        results_exp[file_id].get();
    }

    cout<< "Merging Files... "<<endl;
    merge_files(gen_trace_dir.string()+"/IDtrace");
    merge_files(gen_trace_dir.string()+"/GENtrace");

    cout<<"Generation Finished. Enjoy :)" << endl;
    return;
}


void tracer::f_pg_st(fs::path ref_file, uint32_t id, string gen_trace_dir, boost::unordered_map<addr_5tup, pair<uint32_t, addr_5tup> > * map_ptr) const {
    cout << "Processing " << ref_file.c_str() << endl;
    io::filtering_istream in;
    in.push(io::gzip_decompressor());
    ifstream infile(ref_file.c_str());
    in.push(infile);

    stringstream ss;
    ss << gen_trace_dir<< "/IDtrace/ptrace-"<<id<<".gz";
    io::filtering_ostream out_id;
    out_id.push(io::gzip_compressor());
    ofstream outfile_id (ss.str().c_str());
    out_id.push(outfile_id);
    out_id.precision(15);

    stringstream ss1;
    ss1 << gen_trace_dir<< "/GENtrace/ptrace-"<<id<<".gz";
    io::filtering_ostream out_loc;
    out_loc.push(io::gzip_compressor());
    ofstream outfile_gen (ss1.str().c_str());
    out_loc.push(outfile_gen);
    out_loc.precision(15);

    for (string str; getline(in, str); ) {
        addr_5tup packet (str); // readable;
        auto iter = map_ptr->find(packet);
        if (iter != map_ptr->end()) {
            packet.copy_header(iter->second.second);
            //packet = iter->second.second;
            //packet.timestamp = iter->first.timestamp;
            out_id << packet.timestamp << "\t" << iter->second.first<<endl;
            out_loc << packet.str_easy_RW() << endl;
        }
    }
    cout << " Finished Processing " << ref_file.c_str() << endl;
    in.pop();
    out_id.pop();
    out_loc.pop();
}

/* flow_arr_mp
 * input: string ref_trace_dir: pcap reference trace
 * 	  string flow_info_str: output trace flow first packet infol
 * output:unordered_set<addr_5tup> : the set of first packets of all flows
 *
 * function_brief:
 * obtain first packet of each flow for later flow based pruning
 */
boost::unordered_set<addr_5tup> tracer::flow_arr_mp(string flow_info_str) const {
    vector<fs::path> to_proc_files = get_proc_files(ref_trace_dir_str);
    cout << "Processing ... To process trace files " << to_proc_files.size() << endl;
    // process using multi-thread;
    vector< std::future<boost::unordered_set<addr_5tup> > > results_exp;
    for (uint32_t file_id = 0; file_id < to_proc_files.size(); file_id++) {
        results_exp.push_back(std::async(std::launch::async, &tracer::f_arr_st, this, to_proc_files[file_id]));
    }
    vector< boost::unordered_set<addr_5tup> >results;
    for (uint32_t file_id = 0; file_id < to_proc_files.size(); file_id++) {
        boost::unordered_set<addr_5tup> res = results_exp[file_id].get();
        results.push_back(res);
    }

    // merge the results;
    boost::unordered_set<addr_5tup> flowInfo_set;
    for (uint32_t file_id = 0; file_id < to_proc_files.size(); file_id++) {
        boost::unordered_set<addr_5tup> res = results[file_id];
        for ( boost::unordered_set<addr_5tup>::iterator iter = res.begin(); iter != res.end(); iter++) {
            auto ist_res = flowInfo_set.insert(*iter);
            if (!ist_res.second) { // update timestamp;
                if (iter->timestamp < ist_res.first->timestamp) {
                    addr_5tup rec = *ist_res.first;
                    rec.timestamp = iter->timestamp;
                    flowInfo_set.insert(rec);
                }
            }
        }
    }

    // print the results;
    ofstream outfile(flow_info_str);
    outfile.precision(15);
    for (boost::unordered_set<addr_5tup>::iterator iter = flowInfo_set.begin(); iter != flowInfo_set.end(); ++iter) {
        outfile<< iter->str_easy_RW() <<endl;
    }
    outfile.close();
    return flowInfo_set;
}

/* f_arr_st
 * input: string ref_file: trace file path
 * output:unordered_set<addr_5tup> : pairtial set of all arrival packet;
 *
 * function_brief:
 * single thread process of flow_arr_mp
 */
boost::unordered_set<addr_5tup> tracer::f_arr_st(fs::path ref_file) const {
    cout<<"Procssing " << ref_file.c_str() << endl;
    boost::unordered_set<addr_5tup> partial_flow_rec;
    io::filtering_istream in;
    in.push(io::gzip_decompressor());
    ifstream infile(ref_file.c_str());
    in.push(infile);
    for (string str; getline(in, str); ) {
        addr_5tup packet(str); // readable
        if (packet.timestamp > terminT)
            break;
        partial_flow_rec.insert(packet);
    }
    io::close(in);
    cout<<"Finished procssing " << ref_file.c_str() << endl;
    return partial_flow_rec;
}

/* packet_count_mp
 *
 * input: string real_trace_dir: directory of real traces
 * 	  string packet_count_file: output of packet counts
 *
 * function brief
 * counting the packets for each flow using multi-thread
 */
void tracer::packet_count_mp(string real_trace_dir, string packet_count_file) {
    fs::path dir(real_trace_dir);
    fs::directory_iterator end;
    boost::unordered_map<addr_5tup, uint32_t> packet_count_map;
    uint32_t cpu_count = count_proc();
    mutex mtx;
    atomic_uint thr_n(0);
    atomic_bool reachend(false);
    if (fs::exists(dir) && fs::is_directory(dir)) {
        fs::directory_iterator iter(dir);
        while (iter != end) {
            if ((!reachend) && fs::is_regular_file(iter->status())) { // create thread
                std::thread (&tracer::p_count_st, this, iter->path(), &thr_n, &mtx, &packet_count_map, &reachend).detach();
                thr_n++;
            }
            iter++;

            while (thr_n >= cpu_count) // block thread creation
                std::this_thread::yield();
            while (thr_n > 0 && iter == end) // give time for thread to terminate
                std::this_thread::yield();
        }
    }

    cout << "finished processing" <<endl;
    // sleep for a while for last thread to close
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // debug
    uint32_t dbtotal = 0;
    ofstream ff(packet_count_file);
    for(Map_iter iter = packet_count_map.begin(); iter != packet_count_map.end(); iter++) {
        ff<<iter->first.str_easy_RW()<<"\t"<<iter->second<<endl;
        dbtotal+=iter->second;
    }
    cout<<dbtotal<<endl;
    ff.close();
    cout << "finished copy" <<endl;
    return;
}

/* p_count_st
 *
 * input: string gz_file_ptr gz_file_ptr: file path of real traces
 * 	  atomic_unit * thr_n_ptr: control the concurrent thread no.
 * 	  mutex * mtx: control the shared access of pc_map
 * 	  unordered_map<addr_5tup, uint32_t> * pc_map_ptr: merge the results of counting
 * 	  atmoic_bool * reachend_ptr: indicate the termination of trace gen
 *
 * function brief
 * this is the thread function that produce packet counts
 */
void tracer::p_count_st(const fs::path gz_file_ptr, atomic_uint * thr_n_ptr, mutex * mtx, boost::unordered_map<addr_5tup, uint32_t> * pc_map_ptr, atomic_bool * reachend_ptr) {
    cout<<"Processing:"<<gz_file_ptr.c_str()<<endl;
    uint32_t counter = 0;
    boost::unordered_map<addr_5tup, uint32_t> packet_count_map;
    try {
        io::filtering_istream in;
        in.push(io::gzip_decompressor());
        ifstream infile(gz_file_ptr.c_str());
        in.push(infile);
        for (string str; getline(in, str); ) {
            addr_5tup packet(str);
            if (packet.timestamp > terminT) {
                (*reachend_ptr) = true;
                break;
            }

            counter++;
            auto result = packet_count_map.insert(std::make_pair(packet, 1));
            if (!result.second)
                result.first->second = result.first->second + 1;

        }
        io::close(in);
    } catch (const io::gzip_error & e) {
        cout<<e.what()<<std::endl;
    }

    total_packet += counter;

    std::lock_guard<mutex> lock(*mtx);

    for (Map_iter iter = packet_count_map.begin(); iter != packet_count_map.end(); iter++) {
        auto result = pc_map_ptr->insert(*iter);
        if (!result.second)
            (pc_map_ptr->find(iter->first))->second += iter->second;
    }
    --(*thr_n_ptr);
}


void tracer::parse_pack_file_mp(string pcap_dir, string gen_pack_dir, size_t min, size_t max) const {
    const size_t File_BLK = 3;
    size_t thread_no = count_proc();
    size_t block_no = (max-min + 1)/File_BLK;
    if (block_no * 3 < max-min + 1)
        ++block_no;

    if ( thread_no > block_no) {
        thread_no = block_no;
    }

    size_t task_no = block_no/thread_no;

    vector<string> to_proc;
    size_t thread_id = 1;
    vector< std::future<void> > results_exp;

    fs::path dir(pcap_dir);
    fs::directory_iterator end;
    if (fs::exists(dir) && fs::is_directory(dir)) {
        for (fs::directory_iterator iter(dir); (iter != end); ++iter) {
            if (fs::is_regular_file(iter->status())) {
                if (to_proc.size() < task_no*File_BLK || thread_id == thread_no) {
                    to_proc.push_back(iter->path().string());
                } else {
                    results_exp.push_back(std::async(
                                              std::launch::async,
                                              &tracer::p_pf_st,
                                              this, to_proc,
                                              (thread_id-1)*task_no)
                                         );
		    ++thread_id;
		    to_proc.clear();
                }
            }
        }
    }

    results_exp.push_back(std::async(
                              std::launch::async,
                              &tracer::p_pf_st,
                              this, to_proc,
                              (thread_no-1)*task_no)
                         );
    for (size_t i = 0; i < thread_no; ++i)
	    results_exp[i].get();

    return;
}

void tracer::p_pf_st(vector<string> to_proc, size_t id) const {
    struct pcap_pkthdr header; // The header that pcap gives us
    const u_char *packet; // The actual packet

    pcap_t *handle;
    const struct sniff_ethernet * ethernet;
    const struct sniff_ip * ip;
    const struct sniff_tcp *tcp;
    uint32_t size_ip;
    uint32_t size_tcp;


    size_t count = 2;
    const size_t File_BLK = 3;

    stringstream ss;
    ss<<"packData";
    ss<<std::setw(5)<<std::setfill('0')<<id;
    ss<<"txt.gz";

    ofstream outfile(ss.str());
    io::filtering_ostream out;
    out.push(io::gzip_compressor());
    out.push(outfile);

    for (size_t i = 0; i < to_proc.size(); ++i) {
        if (i > count) {
            out.pop();
            outfile.close();
            ss.clear();
            ++id;
            ss<<"packData";
            ss<<std::setw(5)<<std::setfill('0')<<id;
            ss<<"txt.gz";
            outfile.open(ss.str());
            count += File_BLK;
        }

        char errbuf[PCAP_ERRBUF_SIZE];
        handle = pcap_open_offline(to_proc[i].c_str(), errbuf);

        while (true) {
            packet = pcap_next(handle, &header);
            if (packet == NULL)
                break;

            ethernet = (struct sniff_ethernet*)(packet);
            int ether_offset = 0;
            if (ntohs(ethernet->ether_type) == ETHER_TYPE_IP) {
                ether_offset = 14;
            } else if (ntohs(ethernet->ether_type) == ETHER_TYPE_8021Q) {
                // here may have a bug
                ether_offset = 18;
            } else {
                continue;
            }

            ip = (struct sniff_ip*)(packet + ether_offset);
            size_ip = IP_HL(ip)*4;

            if (IP_V(ip) != 4 || size_ip < 20)
                continue;
            if (uint32_t(ip->ip_p) != 6)
                continue;

            tcp = (struct sniff_tcp*)(packet + ether_offset + size_ip);
            size_tcp = TH_OFF(tcp)*4;
            if (size_tcp < 20)
                continue;

            stringstream ss;
            ss<<header.ts.tv_sec<<'%'<<header.ts.tv_usec<<'%';
            ss<<ntohl(ip->ip_src.s_addr)<<'%'<<ntohl(ip->ip_dst.s_addr);
            ss<<'%'<<tcp->th_sport<<'%'<<tcp->th_dport;

            out<<ss.str()<<endl;
        }

        pcap_close(handle);
    }
    io::close(out);
}



