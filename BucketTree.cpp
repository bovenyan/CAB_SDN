#include "BucketTree.h"

// --------- bucket ------------

using std::vector;
using std::list;
using std::ifstream;
using std::ofstream;


typedef vector<uint32_t>::iterator Iter_id;
typedef vector<bucket*>::iterator Iter_son;

ofstream ff("./log");
bucket::bucket() {}

bucket::bucket(const bucket & bk) : b_rule(bk)
{
    sonList = vector<bucket*>();
    related_rules = vector<uint32_t>();
}

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

string bucket::get_str() const
{
    stringstream ss;
    ss << b_rule::get_str() << "\t" << related_rules.size();
    return ss.str();
}


// ---------- bucket_tree ------------
bucket_tree::bucket_tree()
{
    root = NULL;
    thres_soft = 0;
}

bucket_tree::bucket_tree(rule_list & rL, uint32_t thr, double pa_perc)
{
    thres_hard = thr;
    thres_soft = thr*2;
    rList = &rL;
    root = new bucket(); // full address space
    for (uint32_t i = 0; i < rL.list.size(); i++)
        root->related_rules.insert(root->related_rules.end(), i);
    splitNode(root);
    pa_rule_no = 1500 * pa_perc;
}

bucket_tree::~bucket_tree()
{
    delNode(root);
}

bucket * bucket_tree::search_bucket(const addr_5tup& packet, bucket * searchSon) const
{
    if (searchSon->sonList.size() != 0)
    {
        for (Iter_son iter = searchSon->sonList.begin(); iter != searchSon->sonList.end(); iter++)
        {
            if ((*iter)->packet_hit(packet))
                return search_bucket(packet, *iter);
        }
        return NULL;
    }
    else
    {
        return searchSon;
    }
}

void bucket_tree::print_tree(const string & filename, bool det) const
{
    ofstream out(filename);
    print_bucket(out, root, det);
    out.close();
}


// DFS-construction without pre-allocation
void bucket_tree::splitNode(bucket * ptr)
{
    double cost = ptr->related_rules.size();
    if (cost < thres_soft)
        return;
    double cut_cost;
    uint32_t dim[2];
    uint32_t mindim[2];
    double mincost = cost;

    for (uint32_t i = 0; i < 4; i++)
    {
        for (uint32_t j = i+1; j < 4; j++)
        {
            dim[0] = i;
            dim[1] = j;
            if (ptr->split(dim))
            {
                cut_cost = calCost(ptr);
                if (cut_cost < mincost)
                {
                    mindim[0] = dim[0];
                    mindim[1] = dim[1];
                    mincost = cut_cost;
                }
                // delete trial
                for (Iter_son iter = ptr->sonList.begin(); iter != ptr->sonList.end(); iter++)
                {
                    delete *iter;
                }
                ptr->sonList.clear();
            }
        }
    }
    if (mincost == cost)   // no improvement through delete
    {
        return;
    }
    else
    {
        if (!ptr->split(mindim))
            return;
        calCost(ptr);
        for (Iter_son iter = ptr->sonList.begin(); iter != ptr->sonList.end(); iter++)
        {
            splitNode(*iter);
        }
    }
}

// DFS-PreAllocation-Detection
void bucket_tree::pre_alloc()
{
    vector<uint32_t> rela_buck_count(rList->list.size(), 0);
    INOallocDet(root, rela_buck_count);

    for (uint32_t i = 0; i< pa_rule_no; i++)
    {
        uint32_t count_m = 0;
        uint32_t idx;
        for (uint32_t i = 0; i < rela_buck_count.size(); i++)
        {
            if(rela_buck_count[i] > count_m)
            {
                count_m = rela_buck_count[i];
                idx = i;
            }
        }
        rela_buck_count[idx] = 0;
        pa_rules.insert(idx);
    }

    INOpruning(root);
}

/* obtain the related rules of the sons
 * calculate the cost for a certain cut result
 */
double bucket_tree::calCost(bucket * bk)
{
    double cost = 0;
    for (Iter_son iter_s = bk->sonList.begin(); iter_s != bk->sonList.end(); )
    {
        for (Iter_id iter = bk->related_rules.begin(); iter != bk->related_rules.end(); iter++)
        {
            if ((*iter_s)->match_rule(rList->list[*iter]))
                (*iter_s)->related_rules.insert((*iter_s)->related_rules.end(), *iter);
        }
        if ((*iter_s)->related_rules.empty()) // remove sons with 0 relarule
            bk->sonList.erase(iter_s);
        else
        {
            cost+= (*iter_s)->related_rules.size();
            iter_s++;
        }
    }
    cost = cost/bk->sonList.size();
    return cost;
}

void bucket_tree::delNode(bucket * ptr)
{
    for (Iter_son iter = ptr->sonList.begin(); iter!= ptr->sonList.end(); iter++)
    {
        delNode(*iter);
    }
    delete ptr;
}

void bucket_tree::print_bucket(ofstream & in, bucket * bk, bool detail) const
{
    if (bk->sonList.empty())
    {
        in << bk->get_str() << endl;
        if (detail)
        {
            in << "re: ";
            for (Iter_id iter = bk->related_rules.begin(); iter != bk->related_rules.end(); iter++)
            {
                in << *iter << " ";
            }
            in <<endl;

        }

    }
    else
    {
        for (Iter_son iter = bk->sonList.begin(); iter != bk->sonList.end(); iter++)
            print_bucket(in, *iter, detail);
    }
    return;
}

void bucket_tree::INOallocDet (bucket * bk, vector<uint32_t> & rela_buck_count) const
{
    for (Iter_id iter = bk->related_rules.begin(); iter != bk->related_rules.end(); iter++)
    {
        rela_buck_count[*iter] += 1;
    }
    for (Iter_son iter_s = bk->sonList.begin(); iter_s != bk->sonList.end(); iter_s ++)
    {
        INOallocDet(*iter_s, rela_buck_count);
    }
    return;
}

void bucket_tree::INOpruning (bucket * bk)
{
    for (Iter_id iter = bk->related_rules.begin(); iter != bk->related_rules.end(); )
    {
        if (pa_rules.find(*iter) != pa_rules.end())
            bk->related_rules.erase(iter);
        else
            ++iter;
    }

    if (bk->related_rules.size() < thres_hard)   // if after pruning there's no need to split
    {
        for (Iter_son iter_s = bk->sonList.begin(); iter_s != bk->sonList.end(); iter_s++)
        {
            delNode(*iter_s);
        }
        bk->sonList.clear();
        return;
    }

    for (Iter_son iter_s = bk->sonList.begin(); iter_s != bk->sonList.end(); iter_s ++)
    {
        INOpruning(*iter_s);
    }
    return;
}

