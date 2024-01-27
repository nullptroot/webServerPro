#include "memoryPool.h"
#include <iostream>

MemoryPool::MemoryPool() {}
MemoryPool::~MemoryPool() 
{
    Slot* cur = currentBolck_;
    while(cur) 
    {
        Slot* next = cur->next;
        // free(reinterpret_cast<void *>(cur));
        // 转化为 void 指针，是因为 void 类型不需要调用析构函数,只释放空间
        operator delete(reinterpret_cast<void*>(cur));
        cur = next;
    }
}
void MemoryPool::init(int size) 
{
    assert(size > 0);
    slotSize_ = size;
    currentBolck_ = NULL;
    currentSlot_ = NULL;
    lastSlot_ = NULL;
    freeSlot_ = NULL;
}
// 计算对⻬所需补的空间
inline size_t MemoryPool::padPointer(char* p, size_t align) 
{
    size_t result = reinterpret_cast<size_t>(p);
    return ((align - result) % align);
}
Slot* MemoryPool::allocateBlock() 
{
    /*申请一个大块都是4096*/
    char* newBlock = reinterpret_cast<char *>(operator new(BlockSize));
    /*去除前面槽的大小，剩下的才是可用的内存*/
    char* body = newBlock + sizeof(Slot*);
    // 计算为了对⻬需要空出多少位置
    // size_t bodyPadding = padPointer(body, sizeof(slotSize_));
    /*要保证可用的内存是小块大小的整数倍，因此槽头和真正的内存之间
    有一些区域是没有使用的，下面的就是求这没有使用的区域的大小*/
    size_t bodyPadding = padPointer(body, static_cast<size_t>(slotSize_));
    // 注意：多个线程（eventLoop 共用一个MemoryPool）
    Slot* useSlot;
    {
        // MutexLockGuard lock(mutex_other_);
        std::lock_guard<std::mutex> lock(mutex_other_);
        /*对新申请的内存进行设置，并更新链表*/
        // newBlock接到Block链表的头部
        reinterpret_cast<Slot *>(newBlock)->next = currentBolck_;
        currentBolck_ = reinterpret_cast<Slot *>(newBlock);
        // 为该Block开始的地方加上bodyPadding个char* 空间
        currentSlot_ = reinterpret_cast<Slot *>(body + bodyPadding);
        lastSlot_ = reinterpret_cast<Slot *>(newBlock + BlockSize - slotSize_ + 1);
        useSlot = currentSlot_;
        // slot指针一次移动8个字节
        currentSlot_ += (slotSize_ >> 3);
    }
    return useSlot;
}
Slot* MemoryPool::nofree_solve() 
{
    /*如果当前的槽的块已经使用完了，那么就该再申请一块内存了*/
    if(currentSlot_ >= lastSlot_)
        return allocateBlock();
    /*没有用完，就找到没有用的小块分配，然后更新currentSlot_*/
    Slot* useSlot;
    {
        // MutexLockGuard lock(mutex_other_);
        std::lock_guard<std::mutex> lock(mutex_other_);
        useSlot = currentSlot_;
        currentSlot_ += (slotSize_ >> 3);
    }
    return useSlot;
}
Slot* MemoryPool::allocate() 
{
    /*freeSlot_ 是已经使用过的内存，就是用户使用过，然后释放了
    就会放到这里，这里内存申请会先检查这里有没有内存块，有的话
    直接使用。  这个freeSlot_有大用啊，能保证之前分配的内存可以
    重新使用*/
    if(freeSlot_) 
    {
        {
            // MutexLockGuard lock(mutex_freeSlot_);
            /*多线程访问需要加锁*/
            std::lock_guard<std::mutex> lock(mutex_other_);
            if(freeSlot_) 
            {
                Slot* result = freeSlot_;
                freeSlot_ = freeSlot_->next;
                return result;
            }
        }
    }
    /*freeSlot_里面没有，那么就去真正的槽中申请*/
    return nofree_solve();
}
inline void MemoryPool::deAllocate(Slot* p) 
{
    if(p) 
    {
        // 将slot加入释放队列
        // MutexLockGuard lock(mutex_freeSlot_);
        std::lock_guard<std::mutex> lock(mutex_other_);
        p->next = freeSlot_;
        freeSlot_ = p;
    }
}
MemoryPool& get_MemoryPool(int id) 
{
    static MemoryPool memorypool_[64];
    return memorypool_[id];
}
// 数组中分别存放Slot大小为8，16，...，512字节的BLock链表
/*初始化内存池，分配根据槽的大小分配给每一个hash桶
分配一个MemoryPool指针，这个是静态对象，单例模式*/
void init_MemoryPool() 
{
    for(int i = 0; i < 64; ++i) 
    {
        /*对每一个hash桶的MemoryPool初始化，主要是设置每一块内存的大小*/
        get_MemoryPool(i).init((i + 1) << 3);
        // std::cout<<(i + 1) << 3<<std::endl;
    }
}
// 超过512字节就直接new
void* use_Memory(size_t size) 
{
    if(!size)
        return nullptr;
    /*多余512的内存申请，属于大内存申请，直接让系统调用的new申请即可*/
    if(size > 512)
        return operator new(size);
    // 相当于(size / 8)向上取整
    /*找到可满足size的最小的槽，进行内存申请*/
    return reinterpret_cast<void *>(get_MemoryPool(((size + 7) >> 3) - 1).allocate());
}
void free_Memory(size_t size, void* p) 
{
    if(!p) return;
    /*大于512的内存直接delet即可*/
    if(size > 512) 
    {
        operator delete (p);
        return;
    }
    /*获取对应大小的hash桶*/
    get_MemoryPool(((size + 7) >> 3) - 1).deAllocate(reinterpret_cast<Slot *>(p));
}
