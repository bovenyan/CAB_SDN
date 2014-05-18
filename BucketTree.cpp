#include "BucketTree.h"

// --------- bucket ------------

using std::vector;
using std::list;
using std::ifstream;
using std::ofstream;
using std::pair;

typedef vector<uint32_t>::iterator Iter_id;
typedef vector<bucket*>::iterator Iter_son;
namespace fs = boost::filesystem;
namespace io = boost::iostreams;

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace attrs = boost::log::attributes;

using namespace logging::trivial;

src::severity_logger <severity_level> bucket::lg = src::severity_logger <severity_level>();

void bucket::logger_init() {
    bucket::lg.add_attribute("Class", attrs::constant< string > ("BuckObj "));
}

bucket::bucket():hit(false) {}

bucket::bucket(const bucket & bk) : b_rule(bk) {
    sonList = vector<bucket*>();
    related_rules = vector<uint32_t>();
    hit = false;
}

bucket::bucket(const string & b_str, const rule_list * rL) : b_rule(b_str) {
    for (size_t idx = 0; idx != rL->list.size(); ++idx)
        if (b_rule::match_rule(rL->list[idx]))
            related_rules.push_back(idx);
    hit = false;
}

pair<double, size_t> bucket::split(const vector<size_t> & dim , rule_list *rList) {
    if (!sonList.empty())
        cleanson();

    uint32_t new_masks[4];
    size_t total_son_no = 1;

    for (size_t i = 0; i < 4; ++i) { // new mask
        new_masks[i] = addrs[i].mask;

        for (size_t j = 0; j < dim[i]; ++j) {
            if (~(new_masks[i]) == 0)
                return std::make_pair(-1, 0);

            new_masks[i] = (new_masks[i] >> 1) + (1 << 31);
            total_son_no *= 2;
        }
    }

    sonList.reserve(total_son_no);

    size_t total_rule_no = 0;
    size_t largest_rule_no = 0;

    for (size_t i = 0; i < total_son_no; ++i) {
        bucket * son_ptr = new bucket(*this);

        uint32_t id = i;
        for (size_t j = 0; j < 4; ++j) { // new pref
            son_ptr->addrs[j].mask = new_masks[j];
            size_t incre = (~(new_masks[j]) + 1);
            son_ptr->addrs[j].pref += (id % (1 << dim[j]))*incre;
            id = (id >> dim[j]);
        }

        for (Iter_id iter = related_rules.begin(); iter != related_rules.end(); ++iter) { // rela rule
            if (son_ptr->match_rule(rList->list[*iter]))
                son_ptr->related_rules.push_back(*iter);
        }

        total_rule_no += son_ptr->related_rules.size();
        largest_rule_no = std::max(largest_rule_no, son_ptr->related_rules.size());

        sonList.push_back(son_ptr);
    }
    return std::make_pair(double(total_rule_no)/total_son_no, largest_rule_no);
}

int bucket::reSplit(const vector<size_t> & dim , rule_list *rList) {
    BOOST_LOG_SEV(lg, debug) << "spliting:" << get_str();
    stringstream ss;
    ss<<"related_rules: ";
    for (auto iter = related_rules.begin(); iter!= related_rules.end(); ++iter)
        ss << *iter << " " << endl;


    if (!sonList.empty())
        cleanson();

    uint32_t new_masks[4];
    size_t total_son_no = 1;

    for (size_t i = 0; i < 4; ++i) { // new mask
        new_masks[i] = addrs[i].mask;

        for (size_t j = 0; j < dim[i]; ++j) {
            if (~(new_masks[i]) == 0)
                return -1; // invalid

            new_masks[i] = (new_masks[i] >> 1) + (1 << 31);
            total_son_no *= 2;
        }
    }

    sonList.reserve(total_son_no);
    set<size_t> to_cache_rules;
    int cost = 0;

    for (size_t i = 0; i < total_son_no; ++i) {
        bool to_cache = false;
        bucket * son_ptr = new bucket(*this);

        uint32_t id = i;
        for (size_t j = 0; j < 4; ++j) { // new pref
            son_ptr->addrs[j].mask = new_masks[j];
            size_t incre = (~(new_masks[j]) + 1);
            son_ptr->addrs[j].pref += (id % (1 << dim[j]))*incre;
            id = id >> dim[j];
        }

        BOOST_LOG_SEV(lg, debug) << "son: " << son_ptr->get_str();

        for (Iter_id iter = related_rules.begin(); iter != related_rules.end(); ++iter) { // rela rule
            BOOST_LOG_SEV(lg, debug) << "processing rule: " << *iter;
            if (son_ptr->match_rule(rList->list[*iter])) {
                son_ptr->related_rules.push_back(*iter);

                if (rList->list[*iter].hit) {
                    to_cache = true;
                    ++cost; // one more bucket rule
                }
            }
        }

        if (to_cache) {
            to_cache_rules.insert(son_ptr->related_rules.begin(), son_ptr->related_rules.end());
        }

        sonList.push_back(son_ptr);
    }

    cost +=  to_cache_rules.size();
    return cost;
}

vector<size_t> bucket::unq_comp(rule_list * rList) {
    vector<size_t> result;
    size_t sum = 0;
    for (size_t i = 0; i < 2; ++i) {
        set<size_t> comp;
        for (auto iter = related_rules.begin(); iter != related_rules.end(); ++iter) {
            size_t pref = rList->list[*iter].hostpair[i].pref;
            size_t mask = rList->list[*iter].hostpair[i].mask;
            comp.insert(pref);
            comp.insert(pref+mask);
        }
        result.push_back(comp.size()-1);
        sum += comp.size() - 1;
    }

    for (size_t i = 0; i < 2; ++i) {
        set<size_t> comp;
        for (auto iter = related_rules.begin(); iter != related_rules.end(); ++iter) {
            comp.insert(rList->list[*iter].portpair[i].range[0]);
            comp.insert(rList->list[*iter].portpair[i].range[1]);
        }
        result.push_back(comp.size()-1);
        sum += comp.size() - 1;
    }
    double avg = sum/4;

    vector<size_t> outstand;
    for (size_t i = 0; i < 4; ++i) {
        if (result[i] >= avg)
            outstand.push_back(i);
    }

    return outstand;
}

string bucket::get_str() const {
    stringstream ss;
    ss << b_rule::get_str() << "\t" << related_rules.size();
    return ss.str();
}

void bucket::cleanson() {
    for (auto iter = sonList.begin(); iter != sonList.end(); ++iter)
        delete (*iter);
    sonList.clear();
}

// ---------- bucket_tree ------------
bucket_tree::bucket_tree() {
    root = NULL;
    thres_soft = 0;
}

bucket_tree::bucket_tree(rule_list & rL, uint32_t thr, double pa_perc) {
    thres_hard = thr;
    thres_soft = thr*2;
    rList = &rL;
    root = new bucket(); // full address space
    for (uint32_t i = 0; i < rL.list.size(); i++)
        root->related_rules.insert(root->related_rules.end(), i);

    gen_candi_split();
    splitNode_fix(root);

    pa_rule_no = 1500 * pa_perc;
}

bucket_tree::~bucket_tree() {
    delNode(root);
}

pair<bucket *, int> bucket_tree::search_bucket(const addr_5tup& packet, bucket * buck) const {
    if (!buck->sonList.empty()) {
        size_t idx = 0;
        for (int i = 3; i >= 0; --i) {
            if (buck->cutArr[i] != 0) {
                idx = (idx << buck->cutArr[i]);
                size_t offset = (packet.addrs[i] - buck->addrs[i].pref);
                offset = offset/((~(buck->addrs[i].mask) >> buck->cutArr[i]) + 1);
                idx += offset;
            }
        }
        assert (idx < buck->sonList.size());
        return search_bucket(packet, buck->sonList[idx]);
    } else {
        buck->hit = true;
        int rule_id = -1;

        for (auto iter = buck->related_rules.begin(); iter != buck->related_rules.end(); ++iter) {
            if (rList->list[*iter].packet_hit(packet)) {
                rList->list[*iter].hit = true;
                rule_id = *iter;
                break;
            }
        }
        return std::make_pair(buck, rule_id);
    }
}

bucket * bucket_tree::search_bucket_seri(const addr_5tup& packet, bucket * buck) const {
    if (buck->sonList.size() != 0) {
        for (auto iter = buck->sonList.begin(); iter != buck->sonList.end(); ++iter)
            if ((*iter)->packet_hit(packet))
                return search_bucket_seri(packet, *iter);
        return NULL;
    } else {
        return buck;
    }
}

void bucket_tree::check_static_hit(const b_rule & traf_block, bucket* buck, set<size_t> & cached_rules, size_t & buck_count) const {
    if (buck->sonList.empty()) {
        ++buck_count;
        buck->hit = true;
        for (auto iter = buck->related_rules.begin(); iter != buck->related_rules.end(); ++iter) {
            cached_rules.insert(*iter);
            if (traf_block.match_rule(rList->list[*iter]))
                rList->list[*iter].hit = true;
        }
    } else {
        for (auto iter = buck->sonList.begin(); iter != buck->sonList.end(); ++iter) {
            if ((*iter)->overlap(traf_block))
                check_static_hit(traf_block, *iter, cached_rules, buck_count);
        }
    }
}


void bucket_tree::print_tree(const string & filename, bool det) const {
    ofstream out(filename);
    print_bucket(out, root, det);
    out.close();
}

void bucket_tree::search_test(const string & tracefile_str) const {
    io::filtering_istream in;
    in.push(io::gzip_decompressor());
    ifstream infile(tracefile_str);
    in.push(infile);

    src::severity_logger <severity_level> logger;

    string str;
    cout << "start testing "<< endl; 
    while (getline(in, str)) {
        addr_5tup packet(str, false);
        auto result = search_bucket(packet, root);
        if (result.first != (search_bucket_seri(packet, root))) {
		cout << "error 1" << endl;
            BOOST_LOG_SEV(logger, error) << "Within bucket error: packet: " << str;
            BOOST_LOG_SEV(logger, error) << "search_buck   res : " << result.first->get_str();
            BOOST_LOG_SEV(logger, error) << "search_buck_s res : " << result.first->get_str();

        }
        if (result.second != rList->linear_search(packet)) {
            if (pa_rules.find(rList->linear_search(packet)) == pa_rules.end()) { // not pre-allocated
		cout << "error 2" << endl;
                BOOST_LOG_SEV(logger, error) << "Search rule error: packet:" << str;
                BOOST_LOG_SEV(logger, error) << "search_buck res : " << rList->list[result.second].get_str();
                BOOST_LOG_SEV(logger, error) << "linear_sear res : " << rList->list[rList->linear_search(packet)].get_str();
            }
        }
    }
}

void bucket_tree::static_traf_test(const string & file_str) {
    ifstream file(file_str);
    size_t counter = 0;
    set<size_t> cached_rules;
    size_t buck_count = 0;
    for (string str; getline(file, str); ++counter) {
        vector<string> temp;
        boost::split(temp, str, boost::is_any_of("\t"));
        size_t r_exp = boost::lexical_cast<size_t>(temp.back());
        if (r_exp > 50) {
            --counter;
            continue;
        }

        b_rule traf_blk(str);
        check_static_hit(traf_blk, root, cached_rules, buck_count);
        if (counter > 80)
            break;
    }

    cout << "Cached: " << cached_rules.size() << " rules, " << buck_count << "buckets " <<endl;
    dyn_adjust();

    counter = 0;
    file.seekg(std::ios_base::beg);
    cached_rules.clear();
    buck_count = 0;
    for (string str; getline(file, str); ++counter) {
        vector<string> temp;
        boost::split(temp, str, boost::is_any_of("\t"));
        size_t r_exp = boost::lexical_cast<size_t>(temp.back());
        if (r_exp > 50) {
            --counter;
            continue;
        }

        b_rule traf_blk(str);
        check_static_hit(traf_blk, root, cached_rules, buck_count);
        if (counter > 80)
            break;
    }

    cout << "Cached: " << cached_rules.size() << " rules, " << buck_count << "buckets " <<endl;
}

void bucket_tree::gen_candi_split(size_t cut_no) {
    if (cut_no == 0) {
        vector<size_t> base(4,0);
        candi_split.push_back(base);
    } else {
        gen_candi_split(cut_no-1);
        vector< vector<size_t> > new_candi_split;
        if (cut_no > 1)
            new_candi_split = candi_split;

        for (auto iter = candi_split.begin(); iter != candi_split.end(); ++iter) {
            for (size_t i = 0; i < 4; ++i) {
                vector<size_t> base = *iter;
                ++base[i];
                new_candi_split.push_back(base);
            }
        }
        candi_split = new_candi_split;
    }
}

void bucket_tree::splitNode_fix(bucket * ptr) {

    double cost = ptr->related_rules.size();
    if (cost < thres_soft)
        return;

    pair<double, size_t> opt_cost = std::make_pair(ptr->related_rules.size(), ptr->related_rules.size());
    vector<size_t> opt_cut;

    for (auto iter = candi_split.begin(); iter != candi_split.end(); ++iter) {
        auto cost = ptr->split(*iter, rList);

        if (cost.first < 0)
            continue;

        if (cost.first < opt_cost.first || ((cost.first == opt_cost.first) && (cost.second < opt_cost.second))) {
            opt_cut = *iter;
            opt_cost = cost;
        }
    }

    if (opt_cut.empty())
        return;
    else {
        ptr->split(opt_cut, rList);
        for (size_t i = 0; i < 4; ++i) {
            ptr->cutArr[i] = opt_cut[i];
        }
        for (auto iter = ptr->sonList.begin(); iter != ptr->sonList.end(); ++iter)
            splitNode_fix(*iter);
    }
}

void bucket_tree::pre_alloc() {
    vector<uint32_t> rela_buck_count(rList->list.size(), 0);
    INOallocDet(root, rela_buck_count);

    for (uint32_t i = 0; i< pa_rule_no; i++) {
        uint32_t count_m = 0;
        uint32_t idx;
        for (uint32_t i = 0; i < rela_buck_count.size(); i++) {
            if(rela_buck_count[i] > count_m) {
                count_m = rela_buck_count[i];
                idx = i;
            }
        }
        rela_buck_count[idx] = 0;
        pa_rules.insert(idx);
    }

    INOpruning(root);
}

void bucket_tree::dyn_adjust() {
    merge_bucket(root);
    print_tree("../para_src/tree_merge.dat");
    repart_bucket_fix(root);
    rList->clearHitFlag();
}

void bucket_tree::print_bucket(ofstream & in, bucket * bk, bool detail) const {
    if (bk->sonList.empty()) {
        in << bk->get_str() << endl;
        if (detail) {
            in << "re: ";
            for (Iter_id iter = bk->related_rules.begin(); iter != bk->related_rules.end(); iter++) {
                in << *iter << " ";
            }
            in <<endl;

        }

    } else {
        for (Iter_son iter = bk->sonList.begin(); iter != bk->sonList.end(); iter++)
            print_bucket(in, *iter, detail);
    }
    return;
}

void bucket_tree::INOallocDet (bucket * bk, vector<uint32_t> & rela_buck_count) const {
    for (Iter_id iter = bk->related_rules.begin(); iter != bk->related_rules.end(); iter++) {
        rela_buck_count[*iter] += 1;
    }
    for (Iter_son iter_s = bk->sonList.begin(); iter_s != bk->sonList.end(); iter_s ++) {
        INOallocDet(*iter_s, rela_buck_count);
    }
    return;
}

void bucket_tree::INOpruning (bucket * bk) {
    for (Iter_id iter = bk->related_rules.begin(); iter != bk->related_rules.end(); ) {
        if (pa_rules.find(*iter) != pa_rules.end())
            bk->related_rules.erase(iter);
        else
            ++iter;
    }

    if (bk->related_rules.size() < thres_hard) { // if after pruning there's no need to split
        for (Iter_son iter_s = bk->sonList.begin(); iter_s != bk->sonList.end(); iter_s++) {
            delNode(*iter_s);
        }
        bk->sonList.clear();
        return;
    }

    for (Iter_son iter_s = bk->sonList.begin(); iter_s != bk->sonList.end(); iter_s ++) {
        INOpruning(*iter_s);
    }
    return;
}

void bucket_tree::delNode(bucket * ptr) {
    for (Iter_son iter = ptr->sonList.begin(); iter!= ptr->sonList.end(); iter++) {
        delNode(*iter);
    }
    delete ptr;
}

// dynamic related
void bucket_tree::merge_bucket(bucket * ptr) { // merge using back order search
    if (!ptr->sonList.empty()) {
        for (auto iter = ptr->sonList.begin(); iter!= ptr->sonList.end(); ++iter) {
            merge_bucket(*iter);
        }
    } else
        return;

    // totally greedy merge
    for (auto iter = ptr->sonList.begin(); iter != ptr->sonList.end(); ++iter) {
        if (!((*iter)->hit || (*iter)->related_rules.empty()))
            return;
    }

    for (auto iter = ptr->sonList.begin(); iter != ptr->sonList.end(); ++iter) {
        delete *iter;
    }
    ptr->sonList.clear();
    ptr->hit = true;
}

void bucket_tree::repart_bucket_fix(bucket * ptr) { // repartition the bucket
    if (!ptr->sonList.empty()) {
        for (auto iter = ptr->sonList.begin(); iter != ptr->sonList.end(); ++iter)
            repart_bucket_fix(*iter);
    } else {
        if (!ptr->hit) // do not need to modify non-hit buckets
            return;
        ptr->hit = false; // clear the hit tag

        size_t opt_cost = 1 + ptr->related_rules.size();
        vector<size_t> opt_cut;

        for (auto iter = candi_split.begin(); iter != candi_split.end(); ++iter) {
            int cost = ptr->reSplit(*iter, rList);
            if (cost < opt_cost) {
                opt_cut = *iter;
                opt_cost = cost;
            }
        }

        if (opt_cut.empty())
            return;
        else {
            ptr->reSplit(opt_cut, rList);
            for (size_t i = 0; i < 4; ++i) {
                ptr->cutArr[i] = opt_cut[i];
            }
            for (auto iter = ptr->sonList.begin(); iter != ptr->sonList.end(); ++iter)
                repart_bucket_fix(*iter);
        }
    }
}

// deprecated

/* obtain the related rules of the sons
 * calculate the cost for a certain cut result
 */
/*
double bucket_tree::calCost(bucket * bk) {
    double cost = 0;
    for (Iter_son iter_s = bk->sonList.begin(); iter_s != bk->sonList.end(); ) {
        for (Iter_id iter = bk->related_rules.begin(); iter != bk->related_rules.end(); iter++) {
            if ((*iter_s)->match_rule(rList->list[*iter]))
                (*iter_s)->related_rules.insert((*iter_s)->related_rules.end(), *iter);
        }
        if ((*iter_s)->related_rules.empty()) // remove sons with 0 relarule
            bk->sonList.erase(iter_s);
        else {
            cost+= (*iter_s)->related_rules.size();
            iter_s++;
        }
    }
    cost = cost/bk->sonList.size();
    return cost;
}
*/
/*
void bucket_tree::splitNode(bucket * ptr) {
    double cost = ptr->related_rules.size();
    if (cost < thres_soft)
        return;
    double cut_cost;
    uint32_t dim[2];
    uint32_t mindim[2];
    double mincost = cost;

    for (uint32_t i = 0; i < 4; i++) {
        for (uint32_t j = i+1; j < 4; j++) {
            dim[0] = i;
            dim[1] = j;
            if (ptr->split(dim)) {
                cut_cost = calCost(ptr);
                if (cut_cost < mincost) {
                    mindim[0] = dim[0];
                    mindim[1] = dim[1];
                    mincost = cut_cost;
                }
                // delete trial
                for (Iter_son iter = ptr->sonList.begin(); iter != ptr->sonList.end(); iter++) {
                    delete *iter;
                }
                ptr->sonList.clear();
            }
        }
    }
    if (mincost == cost) { // no improvement through delete
        return;
    } else {
        if (!ptr->split(mindim))
            return;
        calCost(ptr);
        for (Iter_son iter = ptr->sonList.begin(); iter != ptr->sonList.end(); iter++) {
            splitNode(*iter);
        }
    }
}
*/


/*
bool bucket::split( const uint32_t (& dim)[2])
{
    if ((~(addrs[dim[0]].mask) == 0) || (~(addrs[dim[1]].mask) == 0)) // check availability
        return false;

    uint32_t mask_old;
    uint32_t mask_new;
    for (uint32_t i = 0; i < 4; i++)   // four sons
    {
        bucket * temp = new bucket(*this);
        for (uint32_t j = 0; j < 4; j++)   // four fields
        {
            if (j == dim[0])
            {
                mask_old = temp->addrs[j].mask;
                mask_new = (mask_old >> 1) + (1 << 31);
                temp->addrs[j].mask = mask_new;
                if (i/2 == 1)
                    temp->addrs[j].pref += (mask_new-mask_old);
            }
            else if (j == dim[1])
            {
                mask_old = temp->addrs[j].mask;
                mask_new = (mask_old >> 1) + (1 << 31);
                temp->addrs[j].mask = mask_new;
                if (i-(i/2)*2 == 1)
                    temp->addrs[j].pref += (mask_new-mask_old);
            }
        }
        sonList.push_back(temp);
    }
    return true;
}
*/
















