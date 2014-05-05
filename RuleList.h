#ifndef RULELIST_H
#define RULELIST_H

#include "stdafx.h"
#include "Address.hpp"
#include "Rule.hpp"
#include <unordered_map>

class rule_list
{
public:
    std::vector<p_rule> list;
    std::unordered_map <uint32_t, std::vector<uint32_t> > dep_map;

    rule_list();
    rule_list(std::string &);
    void obtain_dep();
    r_rule get_micro_rule (const addr_5tup &);

    void rule_dep_analysis();
    void print(const std::string &);
};
#endif
