#ifndef LFUCACHE_H
#define LFUCACHE_H

#include <unordered_map>
#include <assert.h>
#include <string>
#include <stdlib.h>
#include <mutex>
#include <memory>
#include <sys/mman.h>
#include <sys/stat.h>
#include "../threadSaleDataStruct/threadSaleHashMap.h"
#include "../memoryPool/memoryPool.h"
#define LFU_CAPACITY 5
using std::string;

/*这里没有办法使用高性能hash表，因为链表的插入和删除也是需要加锁的*/

/*修改为缓存文件mmap，并使用高性能的hashmap*/


// 链表的节点  
/*这是真实存储的内容，也就是小链表的节点*/
template<typename T>
class Node 
{
    public:
        void setPre(Node* p) { pre_ = p; }
        void setNext(Node* p) { next_ = p; }
        Node* getPre() { return pre_; }
        Node* getNext() { return next_; std::cout << "Next pointer value: " << next_ << std::endl;}
        T& getValue() { return value_; }
    private:
        T value_;
        Node* pre_; 
        Node* next_;
};


// 文件名->文件内容的映射
// struct Key 
// {
//     string key_, value_;
// };
struct fileContent
{
    char *fileAdrress;
    struct stat fileStat;
    fileContent(char *fa,struct stat fs):fileAdrress(fa),fileStat(fs){};
};
/*取消内存映射函数，作为共享指针的删除器*/
static void unmap(fileContent *fc);

struct Key
{
    string key_;
    std::shared_ptr<fileContent> value_;
    Key():key_(""),value_(nullptr){};
    Key(const string &s,fileContent *fc,void (*unmap)(fileContent *)):key_(s),value_(fc,unmap){};
    // ~Key()
    // {
    //     // printf("%s,%p\n",key_.c_str(),value_.get());
    // }
};
// typedef std::shared_ptr<Key_> Key;
typedef Node<Key>* key_node;
// 链表：由多个Node组成
/*这里是一个频度节点，一个频度节点有一群真实存储的节点
这些真实的节点频度都是一样的*/
class KeyList 
{
    public:
        void init(int freq);
        void destory();
        int getFreq();
        void add(key_node& node);
        void del(key_node& node);
        bool isEmpty();
        key_node getLast();
    private:
        int freq_;
        key_node Dummyhead_;
        key_node tail_;
};
typedef Node<KeyList>* freq_node;
/*
典型的双重链表+hash表实现
首先是一个大链表，链表的每个节点都是一个小链表附带一个值表示频度
小链表存的是同一频度下的key value节点。
hash表存key到大链表节点的映射(key,freq_node)和
key到小链表节点的映射(key,key_node).
*/
// 这里没有用 namespace 限定，大家可以注意优化一下。。
// LFU由多个链表组成
class LFUCache 
{
    private:
        freq_node Dummyhead_; // 大链表的头节点，里面每个节点都是小链表的头节点
        size_t capacity_;
        // MutexLock mutex_;
        std::mutex mutex_;
        /*映射真实的数据*/
        // threadsafe_lookup_table<string,key_node> kmap_;
        std::unordered_map<string, key_node> kmap_; // key到keynode的映射
        /*映射频度节点*/
        std::unordered_map<string, freq_node> fmap_; // key到freqnode的映射
        // threadsafe_lookup_table<string,freq_node> fmap_;
        void addFreq(key_node& nowk, freq_node& nowf);
        void del(freq_node& node);
        void init();
    public:
        LFUCache(int capicity);
        ~LFUCache();
        std::shared_ptr<fileContent> get(string& key); // 通过key返回value并进行LFU操作
        void set(string& key, const fileContent&& value); // 更新LFU缓存
        size_t getCapacity() const { return capacity_; }
        // LFUCache& getCache();
};
LFUCache& getCache();
#endif