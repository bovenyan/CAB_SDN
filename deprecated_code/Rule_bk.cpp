#include "Rule.hpp"

using namespace std;

/* ----------------------------- p_rule----------------------------------
 * brief:
 * bucket_rules: two dim are prefix address, the other two are range address
 */

/* constructors
 *
 * options:
 * 	()			default
 * 	(const p_rule &)	copy function
 * 	(const string &)	generate from a string "#srcpref/mask \t dstpref/mask \t ..."
 */
p_rule::p_rule(){}

p_rule::p_rule(const p_rule & pr){
	hostpair[0] = pr.hostpair[0];
	hostpair[1] = pr.hostpair[1];
	portpair[0] = pr.portpair[0];
	portpair[1] = pr.portpair[1];
	proto = pr.proto;
}

p_rule::p_rule(const string & rule_str){
	vector<string> temp;
	boost::split(temp, rule_str, boost::is_any_of("\t"));
	temp[0].erase(0,1);
	hostpair[0] = pref_addr(temp[0]);
	hostpair[1] = pref_addr(temp[1]);
	portpair[0] = range_addr(temp[2]);
	portpair[1] = range_addr(temp[3]);
	proto = true;
}


/* member fuctions
 */
bool p_rule::dep_rule(const p_rule & rl) const{ // check whether a rule is directly dependent
	if (!hostpair[0].match(rl.hostpair[0]))
		return false;
	if (!hostpair[1].match(rl.hostpair[1]))
		return false;
	if (!portpair[0].overlap(rl.portpair[0]))
		return false;
	if (!portpair[1].overlap(rl.portpair[1]))
		return false;

	return true;	
}

pair<p_rule, bool> p_rule::join_rule(p_rule pr) const{ // use this rule to join another
	if (!hostpair[0].truncate(pr.hostpair[0]))
		return make_pair(p_rule(), false);
	if (!hostpair[1].truncate(pr.hostpair[1]))
		return make_pair(p_rule(), false);
	if (!portpair[0].truncate(pr.portpair[0]))
		return make_pair(p_rule(), false);
	if (!portpair[1].truncate(pr.portpair[1]))
		return make_pair(p_rule(), false);
	return make_pair(pr, true);
}

addr_5tup p_rule::get_corner() const{ // generate the corner as did by ClassBench
	addr_5tup header;
	header.addrs[0] = hostpair[0].get_extreme(rand()%2);
	header.addrs[1] = hostpair[1].get_extreme(rand()%2);
	header.addrs[2] = portpair[0].get_extreme(rand()%2);
	header.addrs[3] = portpair[1].get_extreme(rand()%2);
	header.proto = true;
	return header;
}

addr_5tup p_rule::get_random() const{ // generate a random header from a rule
	addr_5tup header;
	header.addrs[0] = hostpair[0].get_random();
	header.addrs[1] = hostpair[1].get_random();
	header.addrs[2] = portpair[0].get_random();
	header.addrs[3] = portpair[1].get_random();
	header.proto = true;
	return header;
}

bool p_rule::packet_hit(const addr_5tup & packet) const{ // check whehter a rule is hit.
	if (!hostpair[0].hit(packet.addrs[0]))
		return false;
	if (!hostpair[1].hit(packet.addrs[1]))
		return false;
	if (!portpair[0].hit(packet.addrs[2]))
		return false;
	if (!portpair[1].hit(packet.addrs[3]))
		return false;
	// ignore proto check
	return true;
}

/* debug & print function
 */
void p_rule::print() const{
	cout<<get_str()<<endl;
}

string p_rule::get_str() const{
	stringstream ss;
	ss<<hostpair[0].get_str()<<"\t";
	ss<<hostpair[1].get_str()<<"\t";
	ss<<portpair[0].get_str()<<"\t";
	ss<<portpair[1].get_str()<<"\t";
	if (proto)
		ss<<"tcp";
	else
		ss<<"udp";
	return ss.str();
}

/* ----------------------------- b_rule----------------------------------
 * brief:
 * bucket_rules: four dimensions are all prefix address
 */


/* constructors
 * option:
 * 	()			default
 * 	(const b_rule &)	copy
 * 	(const string &)	generate from a string "srcpref/mask \t dstpref/mask \t ..."
 */
b_rule::b_rule(){ // constructor default
	addrs[2].mask = ((~0)<<16); // port is limited to 0~65535
	addrs[3].mask = ((~0)<<16); 
}

b_rule::b_rule(const b_rule & br){ // copy constructor
	for(size_t i=0; i<4; i++){
		addrs[i] = br.addrs[i];
	}
	proto = br.proto;
}

b_rule::b_rule(const string & rule_str){ // 
	vector<string> temp;
	boost::split(temp, rule_str, boost::is_any_of("\t"));
	for(size_t i=0; i<4; i++){
		addrs[i] = pref_addr(temp[i]);
	}
}

/* member funcs
 */
bool b_rule::match_rule(const p_rule & rule) const{ // check whether a policy rule is in bucket
	if (!rule.hostpair[0].match(addrs[0]))
		return false;
	if (!rule.hostpair[1].match(addrs[1]))
		return false;
	if (!rule.portpair[0].match(addrs[2]))
		return false;
	if (!rule.portpair[1].match(addrs[3]))
		return false;
	return true;
}

bool b_rule::match_truncate(p_rule & rule) const{ // truncate a policy rule using a bucket
	if (!addrs[0].truncate(rule.hostpair[0]))
		return false;
	if (!addrs[1].truncate(rule.hostpair[1]))
		return false;
	if (!addrs[2].truncate(rule.portpair[0]))
		return false;
	if (!addrs[3].truncate(rule.portpair[1]))
		return false;
	return true;
}

bool b_rule::packet_hit(const addr_5tup & packet) const{ // check packet hit a bucket
	for (size_t i = 0; i < 4; i++){
		if (!addrs[i].hit(packet.addrs[i]))
			return false;
	}
	// ignore proto check
	return true;
}

/* debug & print function
 */
void b_rule::print() const{  // print func
	cout<<get_str()<<endl;
}

string b_rule::get_str() const{  // print func
	stringstream ss;
	for(size_t i = 0; i < 4; i++){
		ss<<addrs[i].get_str()<<"\t";
	}
	return ss.str();
}


/* ----------------------------- r_rule----------------------------------
 * brief:
 * range_rules: four dimensions are all range_addr
 */

/* constructors
 * 
 * options:
 * 	()			default
 * 	(const r_rule &)	copy
 * 	(const p_rule &)	tranform out of a p_rule
 */
r_rule::r_rule(){
	addrs[0] = range_addr(0, ~size_t(0));
	addrs[1] = range_addr(0, ~size_t(0));
	addrs[2] = range_addr(0, (~size_t(0) >> 16));
	addrs[3] = range_addr(0, (~size_t(0) >> 16));
}

r_rule::r_rule(const r_rule & pr){
	for (size_t i = 0; i < 4; ++i)
		addrs[i] = pr.addrs[i];
}

r_rule::r_rule(const p_rule & pr){
	addrs[0] = range_addr(pr.hostpair[0]);
	addrs[1] = range_addr(pr.hostpair[1]);
	addrs[2] = pr.portpair[0];
	addrs[3] = pr.portpair[1];
	proto = pr.proto;
}

/* operator functions
 *
 * for hash based containers
 */

/* member functions
 */
bool r_rule::overlap (const r_rule & rr) const{ // check whether another r_rule overlap with it.
	for (size_t i = 0; i < 4; ++i){
		if (!addrs[i].overlap(rr.addrs[i]))
			return false;
	}
	return true;
}

string r_rule::get_str() const{
	stringstream ss;
	for(size_t i = 0; i < 4; i++){
		ss<<addrs[i].get_str()<<"\t";
	}
	return ss.str();
}



/* ----------------------------- h_rule----------------------------------
 * brief:
 * hot_rules: four dimensions are all prefix rule
 */

h_rule::h_rule(const string & line):b_rule(line){};

h_rule::h_rule(const h_rule & hr):b_rule(hr){
	rela_rule = hr.rela_rule;
}

h_rule::h_rule(const string & str, const vector<p_rule> & rL):b_rule(str){
	cal_rela(rL);
}

h_rule::h_rule(const addr_5tup & center, size_t (& scope)[4]){
	for (size_t i = 0; i < 2; i++){
		if (scope[i] < 32){
			addrs[i].pref = (center.addrs[i] & ((~0)<<scope[i]));
			addrs[i].mask = ((~0)<<scope[i]);
		}
		else{
			addrs[i].pref = 0;
			addrs[i].mask = 0;
		}
	}
	for (size_t i = 2; i < 4; i++){
		if (scope[i] < 16){
			addrs[i].pref = (center.addrs[i] & ((~0)<<scope[i]));
			addrs[i].mask = ((~0)<<scope[i]);
		}
		else{
			addrs[i].pref = 0;
			addrs[i].mask = ((~0)<<16);
		}
	}
}

size_t h_rule::cal_rela(const vector<p_rule> & rList){
	for (size_t i = 0; i < rList.size(); i++){
		p_rule rule = rList[i]; // copy
		if(match_truncate(rule)){
			rela_rule.push_back(rule);	
		}
	}
	return rela_rule.size();
}

addr_5tup h_rule::gen_header(){
	vector<p_rule>::iterator iter = rela_rule.begin();
	advance(iter, rand()%rela_rule.size());
	return iter->get_random();
}
