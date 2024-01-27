#ifndef SAFEHASHMAP_H
#define SAFEHASHMAP_H

#include <iostream>

#include <thread>
#include <vector>
#include <list>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <iterator>
#include <map>
#include <algorithm>
#include <atomic>

template<typename Key,typename Value,typename Hash=std::hash<Key>>
class threadsafe_lookup_table
{
    private:
        class bucket_type
        {
            // friend class threadsafe_lookup_table;
            private:
                typedef std::pair<Key,Value> bucket_value;
                typedef std::list<bucket_value> bucket_data;
                typedef typename bucket_data::iterator bucket_iterator;
                bucket_data data;
                /*shared_mutex支持多个线程读，单个线程改*/
                mutable std::shared_mutex mutex;
                bucket_iterator find_entry_for(Key const &key)
                {
                     return std::find_if(data.begin(),data.end(),[&](bucket_value const& item){return item.first == key;});
                }
            public:
                // bucket_type(){};
                // bucket_type(Key _key, Value _value):data({_key,_value}){};
                /*不做任何改动，是异常安全的*/
                Value value_for(Key const &key,Value const &default_value) 
                {
                    /*共享访问*/
                    std::shared_lock<std::shared_mutex> lock(mutex);
                    bucket_iterator const found_entry = find_entry_for(key);
                    return (found_entry == data.end()) ? default_value : found_entry->second;
                }
                void add_or_update_mapping(Key const &key,Value const &value,std::atomic<int> &counter)
                {
                    /*独占访问*/
                    std::unique_lock<std::shared_mutex> lock(mutex);
                    bucket_iterator const found_entry = find_entry_for(key);
                    if(found_entry == data.end())
                    {
                    /*下面是异常安全的*/
                        data.push_back(bucket_value(key,value));
                        counter++;
                    }
                    else
                    /*这里赋值操作不是异常安全的，留给用户保证赋值操作是异常安全的*/
                        found_entry->second = value;
                }
                /*此函数是异常安全的*/
                void remove_mapping(Key const &key,std::atomic<int> &counter)
                {
                    std::unique_lock<std::shared_mutex> lock(mutex);
                    bucket_iterator const found_entry = find_entry_for(key);
                    if(found_entry != data.end())
                    {
                        counter--;
                        data.erase(found_entry);
                    }
                }
        };
        std::vector<std::unique_ptr<bucket_type>> buckets;
        Hash hasher;
        /*加一个原子变量来统计hash表元素数量*/
        std::atomic<int> counter;
        /*桶的数目固定，因此此函数不需要加锁，后续调用get_bucket也不需要加锁*/
        bucket_type & get_bucket(Key const &key) const
        {
            std::size_t const bucket_index = hasher(key) % buckets.size();
            return *buckets[bucket_index];
        }
    public:
        typedef Key key_type;
        typedef Value mapped_type;
        typedef Hash hash_type;
        int size() const {return counter.load();};
        threadsafe_lookup_table(unsigned num_buckets = 19,Hash const &hasher_ = Hash()):
                                    buckets(num_buckets),hasher(hasher_),counter(0)
        {
            for(unsigned i = 0; i < num_buckets; ++i)
                buckets[i].reset(new bucket_type);
        }
        threadsafe_lookup_table(threadsafe_lookup_table const &other) = delete;
        threadsafe_lookup_table& operator = (threadsafe_lookup_table const &other) = delete;
        Value value_for(Key const &key,Value const &default_value = Value()) const
        {
            /*桶的数目固定，因此此函数不需要加锁，*/
            return get_bucket(key).value_for(key,default_value);
        }
        void add_or_update_mapping(Key const &key,Value const &value)
        {
            /*桶的数目固定，因此此函数不需要加锁，*/
            get_bucket(key).add_or_update_mapping(key,value,counter);
        }
        void remove_mapping(Key const &key)
        {
            /*桶的数目固定，因此此函数不需要加锁，*/
            get_bucket(key).remove_mapping(key,counter);
        }

        std::map<Key,Value> get_map() const
        {
            std::vector<std::unique_lock<std::shared_mutex>> locks;
            for(unsigned i = 0; i < buckets.size(); ++i)
            {
                locks.push_back(std::unique_lock<std::shared_mutex>(buckets[i].mutex));
            }
            std::map<Key,Value> res;
            for(unsigned i = 0; i < buckets.size(); ++i)
            {
                for(auto it = buckets[i].data.begin(); it != buckets[i].data.end(); ++it)
                    res.insert(*it);
            }
            return res;
        }
};
#endif