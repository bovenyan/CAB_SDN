#include "RuleList.h"

using std::ifstream;
using std::ofstream;
using std::string;

/* constructor
 *
 * options:
 * 	()			default
 * 	(string &)		draw from file
 */
rule_list::rule_list() {}

rule_list::rule_list(string & filename)
{
    ifstream file;
    file.open(filename.c_str());
    string sLine = "";
    getline(file, sLine);
    while (!file.eof())
    {
        p_rule sRule(sLine);
        list.push_back(sRule);
        getline(file, sLine);
    }
    file.close();
}

/* member func
 */
void rule_list::obtain_dep()   // obtain the dependency map
{
    for(uint32_t idx = 0; idx < list.size(); ++idx)
    {
        vector <uint32_t> dep_rules;
        for (uint32_t idx1 = 0; idx1 < idx; ++idx1)
        {
            if (list[idx].dep_rule(list[idx1]))
            {
                dep_rules.push_back(idx1);
            }
        }
        dep_map[idx] = dep_rules;
    }
}

r_rule rule_list::get_micro_rule(const addr_5tup & pack)   // get the micro rule for a given packet;
{
    // linear search to find the matching packet
    uint32_t rule_hit_idx = 0;
    for ( ; rule_hit_idx < list.size(); ++rule_hit_idx )
    {
        if (list[rule_hit_idx].packet_hit(pack))
            break;
    }

    if (rule_hit_idx == list.size())
    {
        cout <<"wrong packet"<<endl;
        exit(0);
    }

    // pruning for a micro rule
    r_rule mRule = list[rule_hit_idx];
    for (auto iter = dep_map[rule_hit_idx].begin(); iter != dep_map[rule_hit_idx].end(); ++iter)
    {
        mRule.prune_mic_rule(list[*iter], pack);
    }
    return mRule;
}


/*
 * debug and print
 */
void rule_list::print(const string & filename)
{
    ofstream file;
    file.open(filename.c_str());
    for (vector<p_rule>::iterator iter = list.begin(); iter != list.end(); iter++)
    {
        file<<iter->get_str()<<endl;
    }
    file.close();
}

void rule_list::rule_dep_analysis()
{
    ofstream ff("rule rec");
    for (uint32_t idx = 0; idx < list.size(); ++idx)
    {
        ff<<"rule : "<< list[idx].get_str() << endl;
        for ( uint32_t idx1 = 0; idx1 < idx; ++idx1)
        {
            auto result = list[idx].join_rule(list[idx1]);
            if (result.second)
                ff << result.first.get_str()<<endl;

        }
        ff<<endl;
    }
}
