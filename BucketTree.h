#ifndef BUCKET_TREE
#define BUCKET_TREE

#include "stdafx.h"
#include "Address.hpp"
#include "Rule.hpp"
#include "RuleList.h"
#include <cmath>
#include <list>
#include <set>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/filesystem.hpp>

using std::vector;
using std::pair;
using std::set;
using std::string;
using std::ofstream;

class bucket: public b_rule {
  private:
    static boost::log::sources::severity_logger <boost::log::trivial::severity_level> lg;
    static void logger_init();
  public:
    vector<bucket*> sonList; 		// List of son nodes
    vector<uint32_t> related_rules;	// IDs of related rules in the bucket
    uint32_t cutArr[4];			// how does this node is cut.  e.g. [2,3,0,0] means 2 cuts on dim 0, 3 cuts on dim 1
    bool hit;

  public:
    bucket();
    bucket(const bucket &);
    bucket(const string &, const rule_list *);
    pair<double, size_t> split(const vector<size_t> &, rule_list *);
    int reSplit(const vector<size_t> &, rule_list *);
    vector<size_t> unq_comp(rule_list *);

    string get_str() const;
  private:
    void cleanson();
};

class bucket_tree {
  public:
    bucket * root;
    rule_list * rList;
    uint32_t thres_soft;
    uint32_t thres_hard;
    uint32_t pa_rule_no;
    set<uint32_t> pa_rules;

    // HyperCut related
    size_t max_cut_per_layer;
    double slow_prog_perc;

    vector<vector<size_t> > candi_split;

  public:
    bucket_tree();
    bucket_tree(rule_list &, uint32_t, double = 0.1);
    ~bucket_tree();

    pair<bucket *, int> search_bucket(const addr_5tup &, bucket* ) const;
    bucket * search_bucket_seri(const addr_5tup &, bucket* ) const;
    void check_static_hit(const b_rule &, bucket*, set<size_t> &, size_t &) const;
    void pre_alloc();

    void dyn_adjust();

    void print_tree(const string &, bool = false) const;
    void search_test(const string &) const;
    void static_traf_test(const string &);

  private:
    // static related
    void gen_candi_split(size_t = 2);
    void splitNode_fix(bucket * = NULL);
    void INOallocDet(bucket *, vector<uint32_t> &) const;
    void INOpruning(bucket *);
    void delNode(bucket *);

    // dynamic related
    void merge_bucket(bucket*);
    void repart_bucket_fix(bucket*);

    void print_bucket(ofstream &, bucket *, bool) const;

};

#endif
