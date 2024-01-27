#include "LFUCache.h"

static void unmap(fileContent *fc)
{
    if (fc->fileAdrress)
    {
        // printf("file mmap release\n");
        munmap(fc->fileAdrress, fc->fileStat.st_size);
        deleteElement(fc);
        // m_file_address = 0;
    }
}

void KeyList::init(int freq) 
{
    freq_ = freq;
    // Dummyhead_ = tail_= new Node<Key>();
    Dummyhead_ = newElement<Node<Key>>();
    tail_ = Dummyhead_;
    Dummyhead_->setNext(nullptr);
}
// 删除整个链表
void KeyList::destory() 
{
    while(Dummyhead_ != nullptr) 
    {
        key_node pre = Dummyhead_;
        Dummyhead_ = Dummyhead_->getNext();
        // delete pre;
        deleteElement(pre);
    }
}
int KeyList::getFreq() { return freq_; }
// 将节点添加到链表头部
/*这里是设置同意频度下的小链表进行添加

Dummyhead_是虚拟节点，一直处于头部，不设置*/
void KeyList::add(key_node& node) 
{
    /*Dummyhead_下一个是nullptr说明 尾和头一样，因此设置尾节点*/
    if(Dummyhead_->getNext() == nullptr) 
    {
        /*设置尾指针*/
        // printf("set tail\n");
        tail_ = node;
        // printf("head_ is %p, tail_ is %p\n", Dummyhead_, tail_);
    }
    else 
    {
        /*Dummyhead_和不一样，那么说明链表中有节点*/
        Dummyhead_->getNext()->setPre(node);
        // printf("this node is not the first\n");
    }
    node->setNext(Dummyhead_->getNext());
    node->setPre(Dummyhead_);
    Dummyhead_->setNext(node);
    assert(!isEmpty());
}
// 删除小链表中的节点
void KeyList::del(key_node& node) 
{
    node->getPre()->setNext(node->getNext());
    if(node->getNext() == nullptr) 
    {
        tail_ = node->getPre();
    }
    else 
    {
        node->getNext()->setPre(node->getPre());
    }
}
bool KeyList::isEmpty() 
{
    return Dummyhead_ == tail_;
}
key_node KeyList::getLast() 
{
    return tail_;
}
LFUCache::LFUCache(int capacity) : capacity_(capacity) 
{
    init();
}
LFUCache::~LFUCache() 
{
    while(Dummyhead_) 
    {
        freq_node pre = Dummyhead_;
        Dummyhead_ = Dummyhead_->getNext();
        pre->getValue().destory();
        // delete pre;
        deleteElement(pre);
    }
}
void LFUCache::init() 
{
    // FIXME:缓存的容量动态变化
    // Dummyhead_ = new Node<KeyList>();
    Dummyhead_ = newElement<Node<KeyList>>();
    Dummyhead_->getValue().init(0);
    Dummyhead_->setNext(nullptr);
}
// 更新节点频度：
// 如果不存在下一个频度的链表，则增加一个
// 然后将当前节点放到下一个频度的链表的头位置
void LFUCache::addFreq(key_node& nowk, freq_node& nowf) 
{
    // printf("enter addFreq\n");
    freq_node nxt;
    // FIXME:频数有可能有溢出
    /*频度不连续是因为下面会因为频度里面的链表空了，就删除了*/
    if(nowf->getNext() == nullptr ||
        nowf->getNext()->getValue().getFreq() != nowf->getValue().getFreq() + 1) 
    {
        // 新建一个下一频度的大链表,加到nowf后面
        // nxt = new Node<KeyList>();
        nxt = newElement<Node<KeyList>>();
        /*下面是加一个大链表的节点，不是大链表中的小链表的节点*/
        nxt->getValue().init(nowf->getValue().getFreq() + 1);
        if(nowf->getNext() != nullptr)
            nowf->getNext()->setPre(nxt);
        nxt->setNext(nowf->getNext());
        nowf->setNext(nxt);
        nxt->setPre(nowf);
        // printf("the prefreq is %d, and the cur freq is %d\n", nowf->getValue().getFreq(), nxt->getValue().getFreq();
    }
    else 
    {
        nxt = nowf->getNext();
    }
    /*将真实内容的键值和频度节点加到频度hash表中*/
    // fmap_[nowk->getValue().key_] = nxt;
    /*Node<Key>* nowk 
        typedef Node<Key>* key_node;*/
    // fmap_.add_or_update_mapping(nowk->getValue().key_,nxt);
    fmap_[nowk->getValue().key_] = nxt;
    // 将其从上一频度的小链表删除
    // 然后加到下一频度的小链表中
    if(nowf != Dummyhead_) 
    {
        nowf->getValue().del(nowk);
    }
    nxt->getValue().add(nowk);
    //printf("the freq now is %d\n", nxt->getValue().getFreq());
    assert(!nxt->getValue().isEmpty());
    // 如果该频度的小链表已经空了
    if(nowf != Dummyhead_ && nowf->getValue().isEmpty())
        del(nowf);
}
std::shared_ptr<fileContent> LFUCache::get(string& key) 
{
    if(!capacity_) return nullptr;
    std::lock_guard<std::mutex> lock(mutex_);
    if(fmap_.find(key) != fmap_.end()) 
    // if(fmap_.value_for(key) != nullptr)
    {
        // 缓存命中
        key_node nowk = kmap_[key];
        freq_node nowf = fmap_[key];
        //  key_node nowk = kmap_.value_for(key);
        // freq_node nowf = fmap_.value_for(key);
        // val += nowk->getValue().value_;
        // val = nowk->getValue().value_;
        addFreq(nowk, nowf);
        return nowk->getValue().value_;
    }
    // 未命中
    return nullptr;
}
/*调用这里，肯定是因为前面已经查询缓存里没有key*/
void LFUCache::set(string& key, const fileContent&& val) 
{
    if(!capacity_) return;
    // printf("kmapsize = %d capacity = %d\n", kmap_.size(), capacity_);
    // MutexLockGuard lock(mutex_);
    std::lock_guard<std::mutex> lock(mutex_);
    // 缓存满了
    // 从频度最小的小链表中的节点中删除最后一个节点（小链表中的删除符合LRU）
    if(kmap_.size() == capacity_) 
    {
        // printf("need to delete a Node\n");
        freq_node head = Dummyhead_->getNext();
        key_node last = head->getValue().getLast();
        head->getValue().del(last);
        kmap_.erase(last->getValue().key_);
        fmap_.erase(last->getValue().key_);
        // kmap_.remove_mapping(last->getValue().key_);
        // fmap_.remove_mapping(last->getValue().key_);
        // delete last;
        deleteElement(last);
        // 如果频度最小的链表已经没有节点，就删除
        if(head->getValue().isEmpty()) 
        {
            del(head);
        }
    }
    // key_node nowk = new Node<Key>();
// 使用内存池
    key_node nowk = newElement<Node<Key>>();
    // nowk->getValue().key_ = key;
    // nowk->getValue().value_ = val;
    nowk->getValue().key_ = key;
    nowk->getValue().value_ = std::shared_ptr<fileContent>(newElement<fileContent>(val),unmap);
    // 具体的实现代码对着图也比较好理解，我这里就不再赘述了，值得注意的是，这里使用的是单例模式（想想为什
    // 么）。这里我提供一个简单的测试 demo。
    addFreq(nowk, Dummyhead_);
    kmap_[key] = nowk;
    fmap_[key] = Dummyhead_->getNext();
    // kmap_.add_or_update_mapping(key,nowk);
    // fmap_.add_or_update_mapping(key,Dummyhead_->getNext());
}
void LFUCache::del(freq_node& node) 
{
    node->getPre()->setNext(node->getNext());
    if(node->getNext() != nullptr) 
    {
        node->getNext()->setPre(node->getPre());
    }
    node->getValue().destory();
    // delete node;
    deleteElement(node);
}
LFUCache& getCache() 
{
    init_MemoryPool();
    static LFUCache cache(LFU_CAPACITY);
    return cache;
}