#include "Address.h"

// ---------------------- Addr_5tup ---------------------

using namespace std;
inline addr_5tup::addr_5tup(){
	for (size_t i = 0; i < 4; i++)
		addrs[i] = 0;
	proto = true;
	timestamp = 0;
}

inline addr_5tup::addr_5tup(const addr_5tup & ad){
	for (size_t i = 0; i < 4; i++)
		addrs[i] = ad.addrs[i];
	proto = ad.proto;
	timestamp = ad.timestamp;
}

inline addr_5tup::addr_5tup(const string & str, bool readable){
	vector<string> temp;
	boost::split(temp, str, boost::is_any_of("%"));
	// ts
	timestamp = boost::lexical_cast<double>(temp[0]);
	proto = true;
	// ip
	if (readable){
		vector<string> temp1;
		boost::split(temp1, temp[1], boost::is_any_of("."));
		addrs[0] = 0;
		for(size_t i=0; i<4; i++){
			addrs[0] = (addrs[0]<<8) + boost::lexical_cast<size_t>(temp1[i]);
		}
		boost::split(temp1, temp[2], boost::is_any_of("."));
		addrs[1] = 0;
		for(size_t i=0; i<4; i++){
			addrs[1] = (addrs[1]<<8) + boost::lexical_cast<size_t>(temp1[i]);
		}
	}
	else{
		addrs[0] = boost::lexical_cast<size_t>(temp[1]);
		addrs[1] = boost::lexical_cast<size_t>(temp[2]);
	}
	// port
	addrs[2] = boost::lexical_cast<size_t>(temp[3]);
	addrs[3] = boost::lexical_cast<size_t>(temp[4]);
	// proto neglect	
}

inline addr_5tup::addr_5tup(const string & str, double ts){
	vector<string> temp;
	boost::split(temp, str, boost::is_any_of("\t"));
	timestamp = ts;
	proto = true;
	for (size_t i = 0; i < 4; i++){
		addrs[i] = boost::lexical_cast<size_t>(temp[i]);
	}
}

inline void addr_5tup::copy_header(const addr_5tup & ad){
	for (size_t i = 0; i < 4; i++)
		addrs[i] = ad.addrs[i];
	proto = ad.proto;
}


inline bool addr_5tup::operator==(const addr_5tup & rhs) const{
	for (size_t i = 0; i < 4; i++){
		if (addrs[i] != rhs.addrs[i])
			return false;
	}
	return (proto == rhs.proto);
}

inline size_t hash_value(addr_5tup const & packet){
	size_t seed = 0;
	boost::hash_combine(seed, packet.addrs[0]);
	boost::hash_combine(seed, packet.addrs[1]);
	boost::hash_combine(seed, packet.addrs[2]);
	boost::hash_combine(seed, packet.addrs[3]);
	return seed;
}

inline string addr_5tup::str_readable() const{
	stringstream ss;
	ss.precision(15);
	ss<<timestamp<<"%";
	for (size_t i = 0; i < 2; i++){
		for (size_t j = 0; j < 4; j++){
			ss << ((addrs[i] >> (24-8*j)) & ((1<<8)-1));
			if (j!=3)
				ss<<".";
		}
		ss<<"%";
	}
	for (size_t i = 2; i < 4; i++)
		ss<<addrs[i]<<"%";

	if (proto)
		ss<<"6";
	else
		ss<<"13";
	return ss.str();
}

inline string addr_5tup::str_easy_RW() const{
	stringstream ss;
	ss.precision(15);
	ss<<timestamp<<"%";
	for (size_t i = 0; i < 4; i++){
		ss<<addrs[i]<<"%";
	}
	if (proto)
		ss<<"1";
	else
		ss<<"0";
	return ss.str();
}



/* Constructor with string input
 * parse the string in form of 
 * 127.0.0.1 \t 10.0.0.1 \t 1023 \t 24 \t tcp 
 * to 5tup
*/
/*
addr_5tup::addr_5tup(string addr_str){

}*/



// ---------------------- pref_addr ---------------------

inline pref_addr::pref_addr(){
	pref = 0;
	mask = 0;
}

inline pref_addr::pref_addr(const string & prefstr){
	vector<string> temp1;
	boost::split(temp1, prefstr, boost::is_any_of("/"));
	
	size_t maskInt = boost::lexical_cast<size_t>(temp1[1]);
	mask = 0;
	if (maskInt != 32)
		mask = ((~size_t(0)) << (32-maskInt));
	else
		mask = ~mask;

	vector<string> temp2;
	boost::split(temp2, temp1[0], boost::is_any_of("."));

	pref = 0;
	for(size_t i=0; i<4; i++){
		pref = (pref<<8) + boost::lexical_cast<size_t>(temp2[i]);
	}
	pref=(pref & mask);
}

inline pref_addr::pref_addr(const pref_addr & pa){
	pref = pa.pref;
	mask = pa.mask;
}

inline bool pref_addr::hit(const size_t & ad) const{
	return (pref == (ad & mask));
}

inline bool pref_addr::match(const pref_addr & ad) const{
	size_t mask_short; 
	if (mask > ad.mask)
		mask_short = ad.mask; 
	else
		mask_short = mask;
	
	return ((pref & mask_short) == (ad.pref & mask_short));
}

inline size_t pref_addr::get_extreme(bool hi) const{
	if (hi)
		return (pref+(~mask));
	else
		return pref;
}

inline size_t pref_addr::get_random() const{
	if (!(~mask+1))
		return pref;
	return (pref + rand()%(~mask+1));
}

inline bool pref_addr::truncate(pref_addr & rule) const{
	if (rule.mask < mask){  // trunc
		if (rule.pref == (pref & rule.mask)){
			rule.mask = mask;
			rule.pref = pref;
			return true;
		}
		else
			return false;
	}
	else{
		if ((rule.pref & mask) == pref)
			return true;
		else
			return false;
	}
}

inline bool pref_addr::truncate(range_addr & rule) const{
	if (rule.range[1] < pref || rule.range[0] > pref+(~mask))
		return false;

	if (rule.range[0] < pref)
		rule.range[0] = pref;
	if (rule.range[1] > pref+(~mask))
		rule.range[1] = pref+(~mask);

	return true;
}

inline void pref_addr::print() const{
	cout<<get_str();
}

inline string pref_addr::get_str() const{
	stringstream ss;
	for (size_t i = 0; i<4; i++){
		ss<<((pref>>(24-(i*8))&((1<<8)-1)));
		if (i != 3)
			ss<<".";
	}
	ss<<"/";
	
	size_t m = 0;
	size_t mask_cp = mask;
	
	if ((~mask_cp) == 0){
		ss<<32;
		return ss.str();
	}
	for (size_t i=0; mask_cp; i++){
		m++;
		mask_cp = (mask_cp << 1);
	}
	ss<<m;
	return ss.str();
}


/* ---------------------- range_addr ---------------------
 * brief:
 * range address: two icons are range start and termin
 */

/* constructors:
 *
 * options:
 * 	()			default
 * 	(const range_addr &)	copy
 * 	(const string &)	generate from a string  "1:1024"
 * 	(const pref_addr &)	transform out of a prefix_addr
 * 	(size_t, size_t)	explicitly initialize with range value
 */
inline range_addr::range_addr(){
	range[0] = 0;
	range[1] = 0;
}

inline range_addr::range_addr(const range_addr & ra){
	range[0] = ra.range[0];
	range[1] = ra.range[1];
}

inline range_addr::range_addr(const string & rangestr){
	vector<string> temp1;
	boost::split(temp1, rangestr, boost::is_any_of(":"));
	boost::trim(temp1[0]);
	boost::trim(temp1[1]);
	range[0] = boost::lexical_cast<size_t> (temp1[0]);
	range[1] = boost::lexical_cast<size_t> (temp1[1]);
}

inline range_addr::range_addr(const pref_addr & rule){
	range[0] = rule.pref;
	range[1] = rule.pref + (~rule.mask);
}

inline range_addr::range_addr(size_t i, size_t j){
	range[0] = i;
	range[1] = j;
}

/* operator functions
 *
 * for hash_bashed and comparison based use 
 */
inline bool range_addr::operator<(const range_addr & ra) const{
	return range[0]< ra.range[0];
}

inline bool range_addr::operator==(const range_addr & ra) const{
	return ( range[0] == ra.range[0] && range[1] == ra.range[1]); 
}

inline size_t hash_value(range_addr const & ra){
	size_t seed = 0;
	boost::hash_combine(seed, ra.range[0]);
	boost::hash_combine(seed, ra.range[1]);
	return seed;
}

/* member function
 */
inline bool range_addr::overlap(const range_addr & ad) const { // whether two range_addr overlap  sym
	return (!(range[1] < ad.range[0]) || (range[0] > ad.range[1]));
}

inline range_addr range_addr::intersect(const range_addr & ra) const{ // return the join of two range addr  sym
	size_t lhs = range[0] > ra.range[0] ? range[0] : ra.range[0];
	size_t rhs = range[1] < ra.range[1] ? range[1] : ra.range[1];
	return range_addr(lhs, rhs);
}

inline bool range_addr::truncate(range_addr & ra) const{ // truncate a rule using current rule  sym
	if (ra.range[0] > range[1] || ra.range[1] < range[0])
		return false;
	if (ra.range[0] < range[0])
		ra.range[0] = range[0];
	if (ra.range[1] > range[1])
		ra.range[1] = range[1];
	return true;
}

inline bool range_addr::match(const pref_addr & ad) const { // whether a range matchs a prefix  sym
	return (! ((range[1] & ad.mask) < ad.pref || (range[0] & ad.mask) > ad.pref));
}

inline bool range_addr::hit(const size_t & ad) const{ // whether a packet hit 
	return (range[0] <= ad && range[1] >= ad);
}

inline void range_addr::getTighter(const size_t & hit, const range_addr & ra){ // get the micro address
	if (ra.range[0] <= hit && ra.range[0] > range[0])
		range[0] = ra.range[0];

	if (ra.range[1] <= hit && ra.range[1] > range[0])
		range[0] = ra.range[1];

	if (ra.range[0] >= hit && ra.range[0] < range[1])
		range[1] = ra.range[0];

	if (ra.range[1] >= hit && ra.range[1] < range[1])
		range[1] = ra.range[1];
}

vector<range_addr> minus_rav(vector<range_addr> & lhs, vector<range_addr> & rhs){ // minus the upper rules
	vector <range_addr> res;
	sort(lhs.begin(), lhs.end());
	sort(rhs.begin(), rhs.end());
	vector<range_addr>::const_iterator iter_l = lhs.begin();
	vector<range_addr>::const_iterator iter_r = rhs.begin();
	while (iter_l != lhs.end()){
		size_t lb = iter_l->range[0];
		while (iter_r != rhs.end()){
			if (iter_r->range[1] < iter_l->range[0]){
				iter_r++;
				continue;
			}
			if (iter_r->range[0] > iter_l->range[1]){
				break;
			}
			range_addr minus_item = iter_l->intersect(*iter_r);
			if (lb < minus_item.range[0])
				res.insert(res.end(), range_addr(lb, minus_item.range[0]-1));
			lb = minus_item.range[1]+1;
			iter_r++;
		}
		if (lb <= iter_l->range[1])
			res.insert(res.end(), range_addr(lb, iter_l->range[1]));
		iter_l++;
	}
	return res;
}

inline size_t range_addr::get_extreme(bool hi) const{ // get the higher or lower range of the addr
	if (hi)
		return range[1];
	else
		return range[0];
}

inline size_t range_addr::get_random() const{ // get a random point picked 
	return (range[0] + rand()%(range[1]-range[0] + 1));
}



/* print function 
 */
inline void range_addr::print() const {
	cout<< get_str();
}

inline string range_addr::get_str() const{
	stringstream ss;
	ss<<range[0] << ":" << range[1];
	return ss.str();
}
