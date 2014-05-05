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
tracer::tracer():offset(346.844), total_packet(0)
{
    rList = NULL;
    flow_no = 0;
}

tracer::tracer(rule_list * rL):offset(346.844), total_packet(0)
{
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
void tracer::set_para(string loc_para_str)
{
    ifstream ff(loc_para_str);
    for (string str; getline(ff, str); )
    {
        vector<string> temp;
        boost::split(temp, str, boost::is_any_of("\t"));
        if (!temp[0].compare("ref_t"))
        {
            ref_trace_dir_str = temp[1];
        }
        if (!temp[0].compare("hot_c"))
        {
            hotcandi_str = temp[1];
        }
        if (!temp[0].compare("p_arr"))
        {
            data_rate = boost::lexical_cast<double>(temp[1]);
        }
        if (!temp[0].compare("f_arr"))
        {
            flow_rate = boost::lexical_cast<double>(temp[1]);
        }
        if (!temp[0].compare("simu_T"))
        {
            duration = boost::lexical_cast<double>(temp[1]);
            terminT = duration+offset;
        }
        if (!temp[0].compare("hot_no"))
        {
            hotspot_no = boost::lexical_cast<uint32_t>(temp[1]);
        }
        if (!temp[0].compare("hot_arr"))
        {
            hotvtime = boost::lexical_cast<double>(temp[1]);
        }
        if (!temp[0].compare("c_prob"))
        {
            cold_prob = boost::lexical_cast<double>(temp[1]);
        }
        if (!temp[0].compare("hr_thres"))
        {
            hot_rule_thres = boost::lexical_cast<uint32_t>(temp[1]);
        }
        if (!temp[0].compare("h_candi_no"))
        {
            hot_candi_no = boost::lexical_cast<uint32_t>(temp[1]);
        }
        if (!temp[0].compare("he_count"))
        {
            flowInfoFile_str = temp[1];
        }
        if (!temp[0].compare("h_scope"))
        {
            vector<string> temp1;
            boost::split(temp1, temp[1], boost::is_any_of(" "));
            for (uint32_t i = 0; i < 4; i++)
            {
                scope[i] = boost::lexical_cast<uint32_t>(temp1[i]);
            }
        }
    }
}

void tracer::print_setup() const
{
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
void tracer::trace_get_ts(string trace_ts_file)
{
    fs::path dir(ref_trace_dir_str);
    fs::directory_iterator end;
    ofstream ffo(trace_ts_file);
    if (fs::exists(dir) && fs::is_directory(dir))
    {
        for (fs::directory_iterator iter(dir); (iter != end); ++iter)
        {
            if (fs::is_regular_file(iter->status()))
            {
                try
                {
                    io::filtering_istream in;
                    in.push(io::gzip_decompressor());
                    ifstream infile(iter->path().c_str());
                    in.push(infile);
                    string str;
                    getline(in, str);
                    addr_5tup f_packet(str);
                    ffo<<iter->path() << "\t" << f_packet.timestamp <<endl;
                    io::close(in); // careful
                }
                catch (const io::gzip_error & e)
                {
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
vector<fs::path> tracer::get_proc_files ( string ref_trace_dir) const
{
    // find out how many files to process
    fs::path dir(ref_trace_dir);
    fs::directory_iterator end;
    vector<fs::path> to_proc_files;
    if (fs::exists(dir) && fs::is_directory(dir))
    {
        for (fs::directory_iterator iter(dir); iter != end; ++iter)
        {
            try
            {
                if (fs::is_regular_file(iter->status()))
                {
                    io::filtering_istream in;
                    in.push(io::gzip_decompressor());
                    ifstream infile(iter->path().c_str());
                    in.push(infile);
                    string str;
                    getline(in,str);
                    addr_5tup packet(str); // readable
                    if (packet.timestamp > terminT)
                    {
                        io::close(in);
                        break;
                    }
                    to_proc_files.push_back(iter->path());
                    io::close(in);
                }
            }
            catch(const io::gzip_error & e)
            {
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
uint32_t count_proc()
{
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
void tracer::merge_files(string gen_trace_dir) const
{
    fs::path file (gen_trace_dir + "/ref_trace.gz");
    if (fs::exists(file))
        fs::remove(file);

    for (uint32_t i = 0; ; ++i)
    {
        stringstream ss;
        //ss<<gen_trace_dir<<"/ref_trace-"<<i<<".gz";
        ss<<gen_trace_dir<<"/ptrace-"<<i<<".gz";
        fs::path to_merge(ss.str());
        if (fs::exists(to_merge))
        {
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
        }
        else
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
void tracer::hotspot_prob(string sav_str)
{
    uint32_t hs_count = 0;
    ofstream ff (sav_str);
    uint32_t probe_scope[4];
    uint32_t trial = 0;
    while (hs_count < hot_candi_no)
    {
        vector<p_rule>::iterator iter = rList->list.begin();
        advance (iter, rand() % rList->list.size());
        // cout<< "candi rule:" << iter->get_str() << endl;
        addr_5tup probe_center = iter->get_random();

        for (uint32_t i=0; i<4; i++)
            probe_scope[i] = scope[i]/2 + rand()%scope[i];
        h_rule hr (probe_center, probe_scope);
        cout << hr.get_str() << "\t" << hr.cal_rela(rList->list) << endl;
        if (hr.cal_rela(rList->list) >= hot_rule_thres)
        {
            ff << probe_center.str_easy_RW() << "\t" << probe_scope[0] << ":"
               << probe_scope[1] << ":" << probe_scope[2] << ":" <<
               probe_scope[3]<<endl;
            trial = 0;
            ++hs_count;
        }

        ++trial;

        if (trial > 100)
        {
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
void tracer::hotspot_prob_b(string bFile, string sav_str)
{
    uint32_t hs_count = 0;
    ofstream ff (sav_str);
    vector <string> file;
    ifstream in (bFile);
    for (string str; getline(in, str); )
    {
        vector<string> temp;
        boost::split(temp, str, boost::is_any_of("\t"));
        if (boost::lexical_cast<uint32_t>(temp.back()) > hot_rule_thres)
        {
            // do some modification here instead of direct push bucket
            file.push_back(str);
        }
    }
    random_shuffle(file.begin(), file.end());
    vector<string>::iterator iter = file.begin();
    while (hs_count < hot_candi_no && iter < file.end())
    {
        ff <<*iter<<endl;
        iter++;
        ++hs_count;
    }
    ff.close();
}


// ===================================== Trace Generation and Evaluation =========================

/* pFlow_pruning_gen
 *
 * input: string ref_trace_dir: real trace directory
 * 	  string genIDtrace: place to put id trace
 * 	  string genLocTrace:place to put generated trace
 *
 * function_brief:
 * wrapper function for generate localized traces
 */
void tracer::pFlow_pruning_gen(string trace_root_dir)
{
    // get the flow info count
    fs::path dir(trace_root_dir + "/Trace_Generate");
    if (fs::create_directory(dir))
    {
        cout<<"creating: " << dir.string()<<endl;
    }
    else
    {
        cout<<"exitst: "<<dir.string()<<endl;
    }

    unordered_set<addr_5tup> flowInfo;
    if (flowInfoFile_str != "")
    {
        ifstream infile(flowInfoFile_str.c_str());
        for (string str; getline(infile, str);)
        {
            addr_5tup packet(str, false);
            if (packet.timestamp > terminT)
                break;
            flowInfo.insert(packet);
        }
        infile.close();
        cout << "read flow info successful" <<endl;
    }
    else
        flowInfo = flow_arr_mp("./para_src/flow_info");

    // trace generated in format of  "trace-200k-0.05-20"
    stringstream ss;
    ss<<dir.string()<<"/trace-"<<flow_rate<<"k-"<<cold_prob<<"-"<<hotspot_no;
    fs::path son_dir(ss.str());
    if (fs::create_directory(son_dir))
    {
        cout<<"creating: "<<son_dir.string()<<endl;
    }
    else
    {
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
void tracer::flow_pruneGen_mp( unordered_set<addr_5tup> & flowInfo, fs::path gen_trace_dir) const
{
    std::multimap<double, addr_5tup> ts_prune_map;
    for (unordered_set<addr_5tup>::iterator iter=flowInfo.begin(); iter != flowInfo.end(); ++iter)
    {
        ts_prune_map.insert(std::make_pair(iter->timestamp, *iter));
    }
    cout << "total flow no. : " << ts_prune_map.size() <<endl;

    // prepair hot spots
    list<h_rule> hotspot_queue;
    ifstream in (hotcandi_str);
    for (uint32_t i = 0; i < hotspot_no; i++)
    {
        string line;
        if (!getline(in, line))
        {
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

    for (auto iter = ts_prune_map.begin(); iter != ts_prune_map.end(); ++iter)
    {
        if (iter->first > next_checkpoint)
        {
            random_shuffle(header_buf.begin(), header_buf.end());
            uint32_t i = 0 ;
            for (i = 0; i < flow_thres && i < header_buf.size(); ++i)
            {
                addr_5tup header;
                if ((double) rand() /RAND_MAX < (1-cold_prob))
                {
                    auto q_iter = hotspot_queue.begin();
                    advance(q_iter, rand()%hotspot_no);
                    header = q_iter->gen_header();
                }
                else
                {
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

        if (iter->first > nextKickOut)
        {
            //ff<<hotspot_queue.front().get_str()<<"\t"<<iter->first<<endl;
            hotspot_queue.pop_front();
            string line;
            if (!getline(in, line))
            {
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

    for(uint32_t file_id = 0; file_id < to_proc_files.size(); ++file_id)
    {
        results_exp.push_back(std::async(std::launch::async, &tracer::f_pg_st, this, to_proc_files[file_id], file_id, gen_trace_dir.string(), &pruned_map));
    }

    for (uint32_t file_id = 0; file_id < to_proc_files.size(); ++file_id)
    {
        results_exp[file_id].get();
    }

    cout<< "Merging Files... "<<endl;
    merge_files(gen_trace_dir.string()+"/IDtrace");
    merge_files(gen_trace_dir.string()+"/GENtrace");

    cout<<"Generation Finished. Enjoy :)" << endl;
    return;
}


void tracer::f_pg_st(fs::path ref_file, uint32_t id, string gen_trace_dir, boost::unordered_map<addr_5tup, pair<uint32_t, addr_5tup> > * map_ptr) const
{
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

    for (string str; getline(in, str); )
    {
        addr_5tup packet (str); // readable;
        auto iter = map_ptr->find(packet);
        if (iter != map_ptr->end())
        {
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
 * obtain the first packet of each flow for later flow based pruning
 */
boost::unordered_set<addr_5tup> tracer::flow_arr_mp(string flow_info_str) const
{
    vector<fs::path> to_proc_files = get_proc_files(ref_trace_dir_str);
    cout << "Processing ... To process trace files " << to_proc_files.size() << endl;
    // process using multi-thread;
    vector< std::future<boost::unordered_set<addr_5tup> > > results_exp;
    for (uint32_t file_id = 0; file_id < to_proc_files.size(); file_id++)
    {
        results_exp.push_back(std::async(std::launch::async, &tracer::f_arr_st, this, to_proc_files[file_id]));
    }
    vector< boost::unordered_set<addr_5tup> >results;
    for (uint32_t file_id = 0; file_id < to_proc_files.size(); file_id++)
    {
        boost::unordered_set<addr_5tup> res = results_exp[file_id].get();
        results.push_back(res);
    }

    // merge the results;
    boost::unordered_set<addr_5tup> flowInfo_set;
    for (uint32_t file_id = 0; file_id < to_proc_files.size(); file_id++)
    {
        boost::unordered_set<addr_5tup> res = results[file_id];
        for ( boost::unordered_set<addr_5tup>::iterator iter = res.begin(); iter != res.end(); iter++)
        {
            auto ist_res = flowInfo_set.insert(*iter);
            if (!ist_res.second)   // update timestamp;
            {
                if (iter->timestamp < ist_res.first->timestamp)
                {
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
    for (boost::unordered_set<addr_5tup>::iterator iter = flowInfo_set.begin(); iter != flowInfo_set.end(); ++iter)
    {
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
boost::unordered_set<addr_5tup> tracer::f_arr_st(fs::path ref_file) const
{
    cout<<"Procssing " << ref_file.c_str() << endl;
    boost::unordered_set<addr_5tup> partial_flow_rec;
    io::filtering_istream in;
    in.push(io::gzip_decompressor());
    ifstream infile(ref_file.c_str());
    in.push(infile);
    for (string str; getline(in, str); )
    {
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
void tracer::packet_count_mp(string real_trace_dir, string packet_count_file)
{
    fs::path dir(real_trace_dir);
    fs::directory_iterator end;
    boost::unordered_map<addr_5tup, uint32_t> packet_count_map;
    uint32_t cpu_count = count_proc();
    mutex mtx;
    atomic_uint thr_n(0);
    atomic_bool reachend(false);
    if (fs::exists(dir) && fs::is_directory(dir))
    {
        fs::directory_iterator iter(dir);
        while (iter != end)
        {
            if ((!reachend) && fs::is_regular_file(iter->status()))   // create thread
            {
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
    for(Map_iter iter = packet_count_map.begin(); iter != packet_count_map.end(); iter++)
    {
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
void tracer::p_count_st(const fs::path gz_file_ptr, atomic_uint * thr_n_ptr, mutex * mtx, boost::unordered_map<addr_5tup, uint32_t> * pc_map_ptr, atomic_bool * reachend_ptr)
{
    cout<<"Processing:"<<gz_file_ptr.c_str()<<endl;
    uint32_t counter = 0;
    boost::unordered_map<addr_5tup, uint32_t> packet_count_map;
    try
    {
        io::filtering_istream in;
        in.push(io::gzip_decompressor());
        ifstream infile(gz_file_ptr.c_str());
        in.push(infile);
        for (string str; getline(in, str); )
        {
            addr_5tup packet(str);
            if (packet.timestamp > terminT)
            {
                (*reachend_ptr) = true;
                break;
            }

            counter++;
            auto result = packet_count_map.insert(std::make_pair(packet, 1));
            if (!result.second)
                result.first->second = result.first->second + 1;

        }
        io::close(in);
    }
    catch (const io::gzip_error & e)
    {
        cout<<e.what()<<std::endl;
    }

    total_packet += counter;

    std::lock_guard<mutex> lock(*mtx);

    for (Map_iter iter = packet_count_map.begin(); iter != packet_count_map.end(); iter++)
    {
        auto result = pc_map_ptr->insert(*iter);
        if (!result.second)
            (pc_map_ptr->find(iter->first))->second += iter->second;
    }
    --(*thr_n_ptr);
}

/* printTestTrace
 * input: tracefile path
 * ouput: void
 *
 * function brief: test how many files are hit by generated trace
 */
void tracer::printTestTrace(string tracefile)
{
    io::filtering_istream ref_trace_stream;
    ref_trace_stream.push(io::gzip_decompressor());
    ifstream ref_trace_file(tracefile);
    ref_trace_stream.push(ref_trace_file);

    double win = 60;
    double checkpoint = win+offset;
    set<uint32_t> header_rec;
    vector<boost::unordered_set<addr_5tup> > flow_rec_vec;
    boost::unordered_set<addr_5tup> flow_rec;
    //uint32_t counter = 0;

    for (string str; getline(ref_trace_stream, str); )
    {
        addr_5tup packet(str, false);
        //counter++;
        //if (counter < 10)
        //	cout<<packet.str_readable()<<endl;
        //else
        //	return;
        if (packet.timestamp > checkpoint)
        {
            cout<< "reached "<<checkpoint<<endl;
            checkpoint+=win;
            flow_rec_vec.push_back(flow_rec);
            flow_rec.clear();
        }
        else
        {
            header_rec.insert(packet.addrs[0]);
            header_rec.insert(packet.addrs[1]);
            flow_rec.insert(packet);
        }
    }
    io::close(ref_trace_stream);

    set<uint32_t> hitrule_rec;
    for (uint32_t i = 0; i<flow_rec_vec.size(); i++)
    {
        stringstream ss;
        ss<<"./TracePruning/TestPlot/snapshot"<<i;
        ofstream ff (ss.str().c_str());
        for (boost::unordered_set<addr_5tup>::iterator iter=flow_rec_vec[i].begin(); iter != flow_rec_vec[i].end(); iter++)
        {
            uint32_t src = distance(header_rec.begin(), header_rec.find(iter->addrs[0]));
            uint32_t dst = distance(header_rec.begin(), header_rec.find(iter->addrs[1]));
            ff<<src<<"\t"<<dst<<endl;
            for (uint32_t j = 0; j < rList->list.size(); j++)
            {
                if (rList->list[j].packet_hit(*iter))
                {
                    hitrule_rec.insert(j);
                    break;
                }
            }
        }
        ff.close();
    }
    cout << hitrule_rec.size() << " rules are hit"<<endl;
}

// single thread deprecated function
// ============================================================================
// ============================================================================
//
void tracer::pruning_trace(string real_trace_dir, string packet_count_file, string gen_trace_file)
{
    // read and pruning header
    ifstream pack_count(packet_count_file);
    vector<pair<addr_5tup, uint32_t> > header_shuffler;
    boost::unordered_map<addr_5tup, uint32_t> pruned_map;

    const uint32_t noise_thres = 10;
    const uint32_t bulk_thres = 0.1*duration*data_rate;

    uint32_t total_packet_count = 0;
    for ( string str; getline(pack_count, str); )
    {
        vector<string> temp;
        boost::split(temp, str, boost::is_any_of("\t"));
        uint32_t count = boost::lexical_cast<uint32_t>(temp[1]);
        if (count < noise_thres || count > bulk_thres)
            continue;
        header_shuffler.push_back(std::make_pair(addr_5tup(temp[0], false), count));
        total_packet_count += count;
    }

    if (total_packet_count < duration*data_rate)
    {
        cout<<"not enough packet, Max rate: "<< total_packet_count/duration <<endl;
        return;
    }

    random_shuffle(header_shuffler.begin(), header_shuffler.end());

    cout <<"finish shuffle"<<endl;
    total_packet_count = 0;
    uint32_t trial = 0;
    uint32_t MaxPacket = duration*data_rate;
    uint32_t flowID = 0;
    for (auto iter = header_shuffler.begin(); iter != header_shuffler.end(); iter++)
    {
        if (total_packet_count + iter->second > MaxPacket)
        {
            trial++;
            if (trial > 3)
                break;
            continue;
        }
        else
        {
            pruned_map.insert(std::make_pair(iter->first, flowID));
            total_packet_count += iter->second;
            ++flowID;
            trial = 0;
        }
    }
    flow_no =  pruned_map.size();
    // cout << "flowNo. " << flowID << ":" << flow_no << endl;
    // cout << "packet No. "<<total_packet_count <<endl;
    assert(flowID == flow_no);
    // prune the trace
    io::filtering_ostream out;
    out.push(io::gzip_compressor());
    ofstream outfile(gen_trace_file);
    out.push(outfile);


    fs::path dir(real_trace_dir);
    fs::directory_iterator end;
    bool reachend = false;
    if (fs::exists(dir) && fs::is_directory(dir))
    {
        for (fs::directory_iterator iter(dir); (iter != end)&&(!reachend); ++iter)
        {
            if (fs::is_regular_file(iter->status()))
            {
                cout<<"Processing:"<<iter->path()<<endl;
                try
                {
                    io::filtering_istream in;
                    in.push(io::gzip_decompressor());
                    ifstream infile(iter->path().c_str());
                    in.push(infile);

                    for (string str; getline(in, str); )
                    {
                        addr_5tup packet(str);
                        if (packet.timestamp > terminT)
                        {
                            reachend = true;
                            break;
                        }
                        Map_iter h_iter = pruned_map.find(packet);
                        if (h_iter != pruned_map.end())
                        {
                            out<<packet.timestamp<<"\t"<<h_iter->second<<endl;
                        }
                    }
                    io::close(in); // careful
                }
                catch (const io::gzip_error & e)
                {
                    cout<<e.what()<<std::endl;
                }
            }
        }
    }
    io::close(out);
}


void tracer::packet_count(string real_trace_dir, string packet_count_file)
{
    // measure the packets for each flow
    fs::path dir(real_trace_dir);
    fs::directory_iterator end;
    boost::unordered_map<addr_5tup, uint32_t> packet_count_map;
    uint32_t total_packet_count = 0;
    bool reachend = false;
    if (fs::exists(dir) && fs::is_directory(dir))
    {
        for (fs::directory_iterator iter(dir); (iter != end)&&(!reachend); ++iter)
        {
            if (fs::is_regular_file(iter->status()))
            {
                cout<<"Processing:"<<iter->path()<<endl;
                try
                {
                    io::filtering_istream in;
                    in.push(io::gzip_decompressor());
                    ifstream infile(iter->path().c_str());
                    in.push(infile);
                    for (string str; getline(in, str); )
                    {
                        addr_5tup packet(str);
                        if (packet.timestamp > terminT)
                        {
                            reachend = true;
                            break;
                        }

                        total_packet_count++;
                        auto result = packet_count_map.insert(std::make_pair(packet, 1));
                        if (!result.second)
                            result.first->second = result.first->second + 1;

                    }
                    io::close(in); // careful
                }
                catch (const io::gzip_error & e)
                {
                    cout<<e.what()<<std::endl;
                }

            }
        }
    }

    // debug
    uint32_t dbtotal = 0;
    ofstream ff(packet_count_file);
    for(Map_iter iter = packet_count_map.begin(); iter != packet_count_map.end(); iter++)
    {
        ff<<iter->first.str_easy_RW()<<"\t"<<iter->second<<endl;
        dbtotal+=iter->second;
    }
    cout<<dbtotal<<endl;
    ff.close();
    return;
}


void tracer::gen_random_trace(string ref_trace, string gen_trace)
{
    fs::path ref_trace_path(ref_trace);
    if (!(fs::exists(ref_trace_path) && fs::is_regular_file(ref_trace_path)))
    {
        cout<<"Missing Ref file"<<endl;
        return;
    }

    uint32_t rListLen = rList->list.size();
    vector<addr_5tup> checkform;
    checkform.reserve(flow_no);
    boost::unordered_set<addr_5tup> u_headers;

    addr_5tup pack;
    for (uint32_t i = 0; i < flow_no; i++)
    {
        do
        {
            pack = rList->list[(rand()%rListLen)].get_random();
        }
        while(u_headers.find(pack) != u_headers.end());
        u_headers.insert(pack);
        checkform.insert(checkform.end(), pack); // append
    }

    // generate the substituted traces
    io::filtering_istream ref_trace_stream;
    ref_trace_stream.push(io::gzip_compressor());
    ifstream ref_trace_file(ref_trace);
    ref_trace_stream.push(ref_trace_file);

    io::filtering_ostream gen_trace_stream;
    gen_trace_stream.push(io::gzip_compressor());
    ofstream gen_trace_file(gen_trace);
    gen_trace_file.precision(16);
    gen_trace_stream.push(gen_trace_file);

    for (string str; getline(ref_trace_stream, str); )
    {
        vector<string> temp;
        boost::split(temp, str, boost::is_any_of("\t"));
        uint32_t id = boost::lexical_cast<uint32_t>(temp[1]);
        checkform[id].timestamp = boost::lexical_cast<double>(temp[0]);
        gen_trace_stream << checkform[id].str_easy_RW() << endl;
    }
}

/*
void tracer::set_para( double d_rate, double dur, uint32_t f_no){
	data_rate = d_rate;
	duration = dur;
	terminT = duration + offset;
	flow_no = f_no;
}
*/
void tracer::p_trace_st(fs::path gz_file_ptr, string gen_trace_str, atomic_uint * thr_n_ptr, boost::unordered_map<addr_5tup, uint32_t> * pc_map_ptr, atomic_bool * reachend)
{
    cout<<"Processing:"<<gz_file_ptr.c_str()<<endl;
    cout<<"output"<<gen_trace_str<<endl;
    try
    {
        io::filtering_istream in;
        in.push(io::gzip_decompressor());
        ifstream infile(gz_file_ptr.c_str());
        in.push(infile);
        io::filtering_ostream out;
        out.push(io::gzip_compressor());
        ofstream outfile(gen_trace_str);
        out.push(outfile);
        for (string str; getline(in, str); )
        {
            addr_5tup packet(str);
            if (packet.timestamp > terminT)
            {
                (*reachend) = true;
                break;
            }
            Map_iter h_iter = pc_map_ptr->find(packet);
            if (h_iter != pc_map_ptr->end())
            {
                out<<packet.timestamp<<"\t"<<h_iter->second<<endl;
            }

        }
        io::close(in);
        io::close(out);
    }
    catch (const io::gzip_error & e)
    {
        cout<<e.what()<<std::endl;
    }
    cout<<"Terminates:"<<gz_file_ptr.c_str()<<endl;
    --(*thr_n_ptr);
}

void tracer::pruning_trace_mp(string real_trace_dir, string packet_count_file, string gen_trace_file)
{
    // read and pruning header
    ifstream pack_count(packet_count_file);
    vector<pair<addr_5tup, uint32_t> > header_shuffler;
    boost::unordered_map<addr_5tup, uint32_t> pruned_map;

    const uint32_t noise_thres = 10;
    const uint32_t bulk_thres = 0.1*duration*data_rate;

    total_packet = 0;
    for ( string str; getline(pack_count, str); )
    {
        vector<string> temp;
        boost::split(temp, str, boost::is_any_of("\t"));
        uint32_t count = boost::lexical_cast<uint32_t>(temp[1]);
        if (count < noise_thres || count > bulk_thres)
            continue;
        header_shuffler.push_back(std::make_pair(addr_5tup(temp[0], false), count));
        total_packet += count;
    }

    uint32_t MaxPacket = duration*data_rate;
    if (total_packet < MaxPacket)
    {
        cout<<"not enough packet, Max rate: "<< total_packet/duration <<endl;
        return;
    }

    random_shuffle(header_shuffler.begin(), header_shuffler.end());
    cout <<"finish shuffle"<<endl;

    total_packet = 0;
    uint32_t trial = 0;
    uint32_t flowID = 0;
    for (auto iter = header_shuffler.begin(); iter != header_shuffler.end(); iter++)
    {
        if (total_packet + iter->second > MaxPacket)
        {
            trial++;
            if (trial > 3)
                break;
            continue;
        }
        else
        {
            pruned_map.insert(std::make_pair(iter->first, flowID));
            total_packet += iter->second;
            ++flowID;
            trial = 0;
        }
    }
    flow_no =  pruned_map.size();
    assert(flowID == flow_no);

    // prune the trace
    fs::path dir(real_trace_dir);
    fs::directory_iterator end;
    uint32_t cpu_count = count_proc()*2;
    atomic_bool reachend(false);
    atomic_uint thr_n(0);
    uint32_t file_id = 0;
    if (fs::exists(dir) && fs::is_directory(dir))
    {
        fs::directory_iterator iter(dir);
        while (iter != end)
        {
            if ((!reachend) && fs::is_regular_file(iter->status()))
            {
                stringstream ss;
                ss<<gen_trace_file<<"-"<<file_id++;
                thread (&tracer::p_trace_st, this, iter->path(), ss.str(), &thr_n, &pruned_map, &reachend).detach();
                thr_n++;
            }
            iter++;

            while (thr_n >= cpu_count)
                std::this_thread::yield();
            while (thr_n > 0 && iter == end)
                std::this_thread::yield();
        }
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
}





void tracer::gen_local_trace(string ref_trace_dir, string gen_trace, string hotspot_candi_file)
{
    list<h_rule> hotspot_queue;
    ifstream in (hotspot_candi_file);
    for (uint32_t i = 0; i < hotspot_no; i++)
    {
        string line;
        if (!getline(in, line))
        {
            in.clear();
            in.seekg(0, std::ios::beg);
        }
        h_rule hr(line, rList->list);
        hotspot_queue.push_back(hr);
    }
    boost::unordered_map<uint32_t, addr_5tup> checkform;
    boost::unordered_set<addr_5tup> u_headers;

    // generate the substituted traces
    io::filtering_ostream gen_trace_stream;
    gen_trace_stream.push(io::gzip_compressor());
    ofstream gen_trace_file(gen_trace);
    gen_trace_file.precision(16);
    gen_trace_stream.push(gen_trace_file);

    double nextKickOut = hotvtime;

    fs::path dir(ref_trace_dir);
    fs::directory_iterator end;
    bool reachend = false;
    uint32_t file_no=0;
    string filePref = "";
    if (fs::exists(dir) && fs::is_directory(dir))
    {
        for (fs::directory_iterator f_iter(dir); (f_iter != end)&&(!reachend); ++f_iter)
        {
            if (fs::is_regular_file(f_iter->status()))
            {
                ++file_no;
                if (!filePref.compare(""))
                    filePref = f_iter->path().string();
            }
        }
    }

    filePref.erase(filePref.size()-1);
    for (uint32_t i = 0; i < file_no; i++)
    {
        stringstream ss;
        ss<<filePref<<i;
        cout<<"Processing:"<< ss.str() <<endl;
        try
        {
            io::filtering_istream ref_trace_stream;
            ref_trace_stream.push(io::gzip_decompressor());
            ifstream ref_trace_file(ss.str().c_str());
            ref_trace_stream.push(ref_trace_file);
            for (string str; getline(ref_trace_stream, str); )
            {
                vector<string> temp;
                boost::split(temp, str, boost::is_any_of("\t"));
                uint32_t id = boost::lexical_cast<uint32_t>(temp[1]);
                if (checkform.find(id) == checkform.end())   // not found
                {
                    addr_5tup pack;
                    if ((double) rand() / RAND_MAX < (1-cold_prob))   // hot
                    {
                        do
                        {
                            list<h_rule>::iterator q_iter = hotspot_queue.begin();
                            advance(q_iter, rand()%hotspot_no);
                            pack = q_iter->gen_header();
                        }
                        while(u_headers.find(pack) != u_headers.end());
                    }
                    else     // cold
                    {
                        do
                        {
                            pack = rList->list[(rand()%(rList->list.size()))].get_random();
                        }
                        while(u_headers.find(pack) != u_headers.end());
                    }
                    checkform[id] = pack;
                }
                checkform[id].timestamp = boost::lexical_cast<double>(temp[0]);
                gen_trace_stream << checkform[id].str_easy_RW() << endl;

                if (checkform[id].timestamp > nextKickOut)
                {
                    hotspot_queue.pop_front();
                    string line;
                    if (!getline(in, line))   // eof
                    {
                        in.clear();
                        in.seekg(0, std::ios::beg);
                        getline(in, line);
                    }
                    h_rule hr(line, rList->list);
                    hotspot_queue.push_back(hr);
                    nextKickOut += hotvtime;
                }
            }
            io::close(ref_trace_stream);
        }
        catch (const io::gzip_error & e)
        {
            cout<<e.what()<<std::endl;
        }
    }
    io::close(gen_trace_stream);
}
