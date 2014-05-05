#ifndef BUCKET_TREE
#define BUCKET_TREE

#include "stdafx.h"
#include "Address.hpp"
#include "Rule.hpp"
#include "RuleList.h"
#include <list>
#include <set>

using std::vector;
using std::set;
using std::string;
using std::ofstream;

class bucket: public b_rule
{
public:
    vector<bucket*> sonList;
    vector<uint32_t> related_rules;

public:
    bucket();
    bucket(const bucket &);
    bool split(const uint32_t (&)[2]);

    //for debug
    string get_str() const;
};

class bucket_tree
{
public:
    bucket * root;
    rule_list * rList;
    uint32_t thres_soft;
    uint32_t thres_hard;

    uint32_t pa_rule_no;
    set<uint32_t> pa_rules;

public:
    bucket_tree();
    bucket_tree(rule_list &, uint32_t, double = 0.1);
    ~bucket_tree();

    bucket * search_bucket(const addr_5tup &, bucket* ) const;
    void pre_alloc();

    void print_tree(const string &, bool = false) const;

private:
    void splitNode(bucket *);
    double calCost(bucket *);
    void delNode(bucket *);
    void print_bucket(ofstream &, bucket *, bool) const;

    void INOallocDet(bucket *, vector<uint32_t> &) const;
    void INOpruning(bucket *);
};

#endif
