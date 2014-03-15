#include "OFswitch.h"


namespace fs = boost::filesystem;
namespace io = boost::iostreams;

using namespace std;

/* Constructor:
 * 
 * method brief:
 * set offset as 346.844 by DOE trace, arrival time of first packet
 */

OFswitch::OFswitch():offset(346.844){
	rList = NULL;
	bTree = NULL;

	mode = 0;
	simuT = 0;
	syn_load = 0;
	TCAMcap = 0;
	tracefile_str = "";
}

/* Constructor:
 * 
 * input: double ofs :  Offset of the traces
 *
 * method brief:
 * same as default 
 */
void OFswitch::set_para(string para_file, rule_list * rL, bucket_tree * bT){
	ifstream ff(para_file);
	rList = rL;
	bTree = bT;
	
	for (string str; getline(ff, str); ){
		vector<string> temp;
		boost::split(temp, str, boost::is_any_of("\t"));
		if (!temp[0].compare("mode")){
			mode = boost::lexical_cast<size_t>(temp[1]);
		}
		if (!temp[0].compare("simuT")){
			simuT = boost::lexical_cast<double>(temp[1]);
		}
		if (!temp[0].compare("load")){
			syn_load = boost::lexical_cast<double>(temp[1]);
		}
		if (!temp[0].compare("TCAMcap")){
			TCAMcap = boost::lexical_cast<size_t>(temp[1]);
		}
		if (!temp[0].compare("tracefile_str")){
			tracefile_str = temp[1];
		}
	}
}

/* run_test
 *
 * method brief:
 * do test according mode id
 * mode 0: CAB, mode 1: Exact Match, mode 2: Cache Dependent Rules, mode 3: Cache micro rules
 */

void OFswitch::run_test(){
	switch (mode){
		case 0:
			CABtest_rt_TCAM();
			break;
		case 1:
			CEMtest_rt_id();
			break;
		case 2:
			CDRtest_rt();
			break;
		case 3:
			CMRtest_rt();
			break;
		default:
			return;
	}
}

/* load_measure
 *
 * method brief:
 * measure the load of a synthetic trace generated
 */
void OFswitch::load_measure(){
	ifstream ff(tracefile_str);
	boost::unordered_set<addr_5tup> header_set;
	double curT = 0;
	double interval = 1/syn_load;
	for (string str; getline(ff, str); ){
		curT += interval;
		addr_5tup packet(str, curT);
		header_set.insert(packet);
		if (curT > simuT+offset)
			break;
	}
	cout << "flow no.:" << header_set.size() <<endl;
}

/* CABtest_rt_TCAM
 *
 * method brief:
 * test caching in bucket
 * trace format <addr5tup(easyRW)>
 * test CAB performance
 */
void OFswitch::CABtest_rt_TCAM(){
	double curT = 0;
	size_t total_packet = 0;

	lru_cache_ex cam_cache(TCAMcap);
	try{
		io::filtering_istream in;
		in.push(io::gzip_decompressor());
		ifstream infile(tracefile_str);
		in.push(infile);
		
		string str; // tune curT to that of first packet 
		getline(in,str);
		addr_5tup first_packet(str, false);
		curT = first_packet.timestamp;

		while(getline(in, str)){
			addr_5tup packet(str, false);
			curT = packet.timestamp;

			bucket* buck = bTree->search_bucket(packet, bTree->root);
			++total_packet;
			if (buck!=NULL){
				cam_cache.ins_rec(buck, curT);
			}
			else
				cout<<"null bucket" <<endl;

			if (curT > simuT+offset)
				break;
		}
	}
	catch (const io::gzip_error & e){
		cout<<e.what()<<endl;
	}

	// generate report
	cout << "cache_miss: " << cam_cache.request_count/simuT << endl;
	cout << "rule_down_no: " << cam_cache.rule_down_count/simuT << endl;
	cout << "bucket_reuse_rate:" << cam_cache.reuse_count/simuT << endl;
	cout << "total_packet:" << total_packet <<endl;
	

	ofstream ff ("delay_rec-1");
	ff << 20 << "\t" << 1 << endl;
	size_t miss = cam_cache.delay_rec[0];

	double scale = 0.0025/miss;
	double res = 1;
	for (int i = 0; i < 21; i++){
		cout << cam_cache.delay_rec[i] << " "<< endl;
		ff << 19 - i <<"\t" << res-cam_cache.delay_rec[i]*scale << endl;
		res -= cam_cache.delay_rec[i]*scale;
		//cout << cam_cache.delay_rec[i] << " ";
	}
	//cout <<endl;
}

/* CEMtest_rt_id
 *
 * method brief:
 * testing cache exact match entries
 * trace format <time \t ID>
 * others same as CABtest_rt_TCAM
 */
void OFswitch::CEMtest_rt_id(){
	fs::path ref_trace_path(tracefile_str);
	if (!(fs::exists(ref_trace_path) && fs::is_regular_file(ref_trace_path))){
		cout<<"Missing Ref file"<<endl;
		return;
	}
	
	double curT = 0;
	lru_cache<size_t, double> flow_table(TCAMcap);
	double cache_miss = 0;
	pair<size_t, bool> kick_info;

	boost::unordered_map<size_t, double> buffer_check;
	size_t delay_rec[21];
	for (int i = 0; i<21; ++i)
		delay_rec[i] = 0;
	size_t total_packet = 0;

	try{
		io::filtering_istream trace_stream;
		trace_stream.push(io::gzip_decompressor());
		ifstream trace_file(tracefile_str);
		trace_stream.push(trace_file);

		// first packet
		string str; 
		getline(trace_stream, str);
		vector<string> temp;
		boost::split(temp, str, boost::is_any_of("\t"));
		curT = boost::lexical_cast<double>(temp[0]);

		while(getline(trace_stream, str)){
			boost::split(temp, str, boost::is_any_of("\t"));
			double time = boost::lexical_cast<double> (temp[0]);
			curT = time;
			++total_packet;

			size_t flow_id = boost::lexical_cast<size_t> (temp[1]);
			if (flow_table.ins_rec( flow_id , curT, kick_info)){
				buffer_check.insert(std::make_pair(flow_id, curT));
				++delay_rec[20];
				cache_miss+=1;
			}
			else{
				if (buffer_check.find(flow_id) != buffer_check.end()){
					double delay = curT - buffer_check.find(flow_id)->second;
					if (delay < 0.006 && delay > 0){
						++delay_rec[20-int(delay/0.0003)];
					}
					else{
						if (delay > 0)
							buffer_check.erase(flow_id);
					}
				}
			}
		}
	}
	catch (const io::gzip_error & e){
		cout<<e.what()<<endl;
	}

	// generate report
	cache_miss = cache_miss/simuT;
	cout << "cache_miss: " << cache_miss<<endl;
	cout << "rule_down_no: " << cache_miss<<endl;
	cout << "total_packet: "<<total_packet<<endl;

	cout << "delay ";
	for (int i =0; i <= 21; ++i)
		cout <<delay_rec[i]<<" ";

	cout <<endl;

}

/* CDRtest_rt
 * 
 * method brief:
 * test caching all dependent rules
 * others same as CABtest_rt_TCAM
 */
void OFswitch::CDRtest_rt(){
	fs::path ref_trace_path(tracefile_str);
	if (!(fs::exists(ref_trace_path) && fs::is_regular_file(ref_trace_path))){
		cout<<"Missing Ref file"<<endl;
		return;
	}

	lru_cache_cdr flow_table(TCAMcap);
	boost::unordered_set<addr_5tup> processed_flow;
	
	double curT = 0;
	size_t cache_miss = 0;
	size_t rule_down_no = 0;
	pair<size_t, bool> kick_info;
	bool processed = false;

	try{
		io::filtering_istream trace_stream;
		trace_stream.push(io::gzip_decompressor());
		ifstream trace_file(tracefile_str);
		trace_stream.push(trace_file);
	
		for (string str; getline(trace_stream, str); ){
			addr_5tup packet(str, false);
			curT = packet.timestamp;

			for (size_t idx = 0; idx < rList->list.size(); idx++){ // find the match rule
				if ((rList->list[idx]).packet_hit(packet)){
					if (processed_flow.insert(packet).second)
						processed = false;
					else 
						processed = true;
					
					bool miss_one = false;
					flow_table.ins_rec(idx, curT, kick_info, processed);
					if (kick_info.second){
						miss_one = true;
						++rule_down_no;
					}
					for (size_t idx_dp = 0; idx_dp < rList->dep_map[idx].size(); ++idx_dp){
						flow_table.ins_rec(rList->dep_map[idx][idx_dp], curT, kick_info, processed);
						if (kick_info.second){
							miss_one = true;
							++rule_down_no;
						}
					}
					if (miss_one)
						++cache_miss;
					break;
				}
			}
			
			if (curT > simuT+offset)
				break;
		}
	}
	catch (const io::gzip_error & e){
		cout<<e.what()<<endl;
	}

	// generate report
	cout << "cache_miss: " << double(cache_miss)/simuT << endl;
	cout << "rule_down_no: " << double(rule_down_no)/simuT << endl;
	cout << "total_flow" << size_t( processed_flow.size()) << endl;

	ofstream ff ("delay_rec");
	ff << 20 <<"\t"<< 1 <<endl;
	size_t total = processed_flow.size();
	double res = 1;
	double scale = (double (150)/983)/flow_table.delay_rec[0];
	for (int i = 0; i < 20; ++i){
		//ff << 19 - i<< "\t"<< double(res-flow_table.delay_rec[i])/total<<endl;
		cout << flow_table.delay_rec [i] <<" " << endl;
		ff << 19 - i << "\t" << res-flow_table.delay_rec[i]*scale<<endl;
		res -= flow_table.delay_rec[i] * scale;
		//res -= flow_table.delay_rec[i];
	}

	cout <<endl;
}


void OFswitch::CMRtest_rt(){
	fs::path ref_trace_path(tracefile_str);
	if (!(fs::exists(ref_trace_path) && fs::is_regular_file(ref_trace_path))){
		cout<<"Missing Ref file"<<endl;
		return;
	}
	
	double curT = 0;
	size_t cache_miss = 0;
	size_t rule_down_no = 0;
	pair<size_t, bool> kick_info;
	bool processed = false;
	
	try{
		io::filtering_istream trace_stream;
		trace_stream.push(io::gzip_decompressor());
		ifstream trace_file(tracefile_str);
		trace_stream.push(trace_file);
		
		for (string str; getline(trace_stream, str); ){
			addr_5tup packet(str, false);
			curT = packet.timestamp;

			r_rule mRule = rList->get_micro_rule(packet);
		}
}



// ----------------------------------------------------------------------------
// deprecated synthetic trace test
/*
void OFswitch::CABtest_buck(){
	ifstream ff(tracefile_str);
	double curT = 0;
	double interval = 1/syn_load;
	
	lru_cache<bucket*, double> bucket_table(TCAMcap);

	cout << tracefile_str <<endl;
	pair<bucket*, bool> kick_info;
	size_t cache_miss = 0;
	size_t rule_down_no = 0;
	for (string str; getline(ff, str); ){
		curT += interval;
		addr_5tup packet(str, curT);
		bucket* buck = bTree->search_bucket(packet, bTree->root);

		if (buck!=NULL){
			if (bucket_table.ins_rec(buck, curT, kick_info)){
				cache_miss ++;
				rule_down_no += buck->related_rules.size();
			}
		}
		else{
			cout << "null bucket" <<endl;
		}

		if (curT > simuT+offset)
			break;
	}

	cache_miss = cache_miss/simuT;
	rule_down_no = rule_down_no/simuT;
	cout << "cache_miss: " << cache_miss<<endl;
	cout << "rule_down_no: " << rule_down_no<<endl; 
}

void OFswitch::CEMtest(){
	ifstream ff(tracefile_str);
	double curT = 0;
	double interval = 1/syn_load;
	lru_cache<addr_5tup, double> flow_table(TCAMcap);
	
	size_t cache_miss = 0;
	pair<addr_5tup, bool> kick_info;
	for (string str; getline(ff, str); ){
		curT+= interval;
		addr_5tup packet(str, curT);
		if (flow_table.ins_rec(packet, curT, kick_info)){
			cache_miss ++;
		}
	}
	
	cache_miss = cache_miss/simuT;
	cout << "cache_miss: " << cache_miss<<endl;
	cout << "rule_down_no: " << cache_miss<<endl;
}

void OFswitch::CMRtest(){
	ifstream ff(tracefile_str);
	double curT = 0;
	double interval = 1/syn_load;
	lru_cache<f_node *, double> flow_table(TCAMcap);

	size_t cache_miss = 0;
	pair<f_node*, bool> kick_info;
	for (string str; getline(ff, str); ){
		curT+= interval;
		addr_5tup packet(str, curT);
		f_node * fd = mTree->search_node(packet); 
		if (flow_table.ins_rec(fd, curT, kick_info)){
			cache_miss ++;
		}
	}
}

*/
