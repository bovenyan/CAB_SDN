#ifndef _LRU_CACHE_H
#define _LRU_CACHE_H

#include <boost/bimap.hpp>
#include <boost/bimap/list_of.hpp>
#include <boost/bimap/set_of.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <boost/function.hpp>
#include <cassert>

#include "BucketTree.h"
#include <boost/unordered_map.hpp>

template <typename K, typename V> 
class lru_cache{
	public:
		typedef K key_type;
		typedef V value_type;
		typedef boost::bimaps::bimap<
			boost::bimaps::unordered_set_of<key_type>,
			boost::bimaps::list_of<value_type>
			> container_type;
		const size_t _capacity;
		container_type _container;
	
		lru_cache(size_t c):_capacity(c){
			assert(_capacity != 0);
		}

		bool ins_rec(const key_type & k, const value_type & v, std::pair<key_type, bool> &kickstat){
			const typename container_type::left_iterator it =
				_container.left.find(k);
			
			if (it==_container.left.end()){ // insert
				kickstat = insert (k,v);
				return true;
			}
			else{ // update
				_container.right.relocate(_container.right.end(),
						_container.project_right(it)
						);
				it->second = v;

				return false;
			}
		}

	private:
		std::pair<key_type, bool> insert(const key_type & k, const value_type & v){
			assert(_container.size()<=_capacity);
			key_type result;
			bool kick = false;
			if (_container.size() == _capacity){
				result = _container.right.begin()->second;
				_container.right.erase(_container.right.begin());
				kick = true;
			}

			_container.insert(
					typename container_type::value_type(k,v)
					);
			
			return std::make_pair(result, kick);
		}
};	


/* class lru_cache_cdr
 *
 * member:
 * 	_capacity:	size_t		flow table capacity
 * 	_container:	bimap		bimap with flowID v.s. time mapping
 * 	delay_rec[21]:	size_t		21 sampling points for flow setup delay
 *	buffer_check:	size_t,double	checking whether a request is on the way
 *
 * methods:
 * 	constructor:
 * 		(size_t): set capacity, initialize delay_rec
 *	ins_rec:
 *		input: 	size_t k		flow ID
 *			double v		time for flow to be inserted
 *			pair<size_t,bool> &	records whether there's kick, how many are kicked
 *			bool proc		whether 
 */			
class lru_cache_cdr{
	public:
		typedef boost::bimaps::bimap<
			boost::bimaps::unordered_set_of<size_t>,
			boost::bimaps::list_of<double>
			> container_type;
		const size_t _capacity;
		container_type _container;
		size_t delay_rec[21];
		boost::unordered_map <size_t , double> buffer_check;
	
		lru_cache_cdr(size_t c):_capacity(c){
			assert(_capacity != 0);
			for (int i = 0; i<21; ++i)
				delay_rec[i] = 0;
		}

		bool ins_rec(const size_t & k, const double & v, std::pair<size_t, bool> &kickstat, bool proc){
			const typename container_type::left_iterator it =
				_container.left.find(k);
			
			if (it==_container.left.end()){ // insert
				//if (kickstat.second)
				//	kickstat = insert (k,v);
				//else{
					kickstat = insert (k,v);
				if (!proc)
					++delay_rec[0]; // cache miss
				//}
				return true;
			}
			else{ // update
				_container.right.relocate(_container.right.end(),
						_container.project_right(it)
						);
				it->second = v;

				//if(kickstat.second){
				if (!proc){
				double delay = v-buffer_check.find(k)->second;
				if (delay< 1.2 && delay > 0){
					delay_rec[20-int(delay/0.06)] += 1;  // delay within 0 - 6
				}
				}
				//}
				return false;
			}
		}

	private:
		std::pair<size_t, bool> insert(const size_t & k, const double & v){
			assert(_container.size()<=_capacity);
			size_t result;
			bool kick = false;
			buffer_check.insert(std::make_pair(k, v));
			if (_container.size() == _capacity){
				result = _container.right.begin()->second;
				_container.right.erase(_container.right.begin());
				buffer_check.erase(result);
				kick = true;
			}

			_container.insert(container_type::value_type(k,v));
			
			return std::make_pair(result, kick);
		}
};	


class lru_cache_ex{
	public:
		// structures
		boost::unordered_map <size_t, size_t> flow_table;
		boost::unordered_map <const bucket*, double> buffer_check;

		typedef boost::bimaps::bimap<
			boost::bimaps::unordered_set_of<const bucket*>,
			boost::bimaps::list_of<double>
			> container_type;
		const size_t _capacity;
		container_type _container; 
		// statistics
		size_t request_count;
		size_t rule_down_count;
		size_t reuse_count;
		size_t delay_rec[21];

		lru_cache_ex(const size_t & cap):_capacity(cap){
			request_count = 0;
			rule_down_count = 0;
			reuse_count = 0;
			for (int i = 0; i<21; ++i)
				delay_rec[i] = 0;
		}

		void ins_rec(const bucket * buck, const double & time){
			const container_type::left_iterator iter = _container.left.find(buck);
			
			if (iter == _container.left.end()){ // insert
				insert(buck, time);
				++delay_rec[0];
			}
			else{ // update
				_container.right.relocate(_container.right.end(),
						_container.project_right(iter));
				iter->second = time;
				reuse_count += 1;
				
				double delay = time-buffer_check.find(buck)->second;
				if (delay< 1.2 && delay > 0){
					delay_rec[20-int(delay/0.06)] += 1;
				}
			}
		}


	private:
		void insert(const bucket* , const double &);
};


#endif
