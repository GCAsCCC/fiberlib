#ifndef IOMANAGER_H
#define IOMANAGER_H

#include <memory>
#include<iostream>
#include"threadPoll_fiber_scheduler.h"
#include"mutex.h"
#include"TimerManager.h"


class IOManager:public scheduler,public TimerManager{

public:
    typedef std::shared_ptr<IOManager> ptr;
    typedef RWMutex RWMutexType;
    /**
     * @brief IO事件，继承自epoll对事件的定义
     * @details 这里只关心socket fd的读和写事件，其他epoll事件会归类到这两类事件中
     */  
    enum Event{
        //无事件
        NONE=0x0,
        //读事件
        READ=0x1,
        //写事件
        WRITE=0x4,
    };


    /// @brief 构造函数
    /// @param threads 线程数量
    /// @param use_caller 是否使用主线程进行任务执行
    /// @param name 调度器的名字
    IOManager(size_t threads = 1,bool use_caller = true,const std::string &name = "");

     /**
     * @brief 析构函数
     */
    ~IOManager();

    /**
     * @brief 添加事件
     * @param[in] fd socket句柄
     * @param[in] event 事件类型
     * @param[in] cb 事件回调函数
     * @return 添加成功返回0,失败返回-1
     */
    int addEvent(int fd, Event event, std::function<void()> cb = nullptr);

    /**
     * @brief 删除事件
     * @param[in] fd socket句柄
     * @param[in] event 事件类型
     * @attention 不会触发事件
     */
    bool delEvent(int fd, Event event);

    /**
     * @brief 取消事件
     * @param[in] fd socket句柄
     * @param[in] event 事件类型
     * @attention 如果事件存在则触发事件
     */
    bool cancelEvent(int fd, Event event);

    /**
     * @brief 取消所有事件
     * @param[in] fd socket句柄
     */
    bool cancelAll(int fd);

    /**
     * @brief 返回当前的IOManager
     */
    static IOManager* GetThis();


private:
    /**
     * @brief socket fd上下文
     * @details 每个socket fd都对应一个FdContext，包括fd的值，fd上的事件，以及fd的读写事件上下文
     */
    struct FdContext
    {
        typedef Mutex MutexType;
        /**
         * @brief 事件上下文
         * @details fd的每个事件都有一个事件上下文，保存这个事件
         *          的回调函数以及执行回调函数的调度器
         */
        struct EventContext
        {
            ///执行事件回调的调度器
            scheduler* m_scheduler=nullptr;
            ///事件回调协程
            std::shared_ptr<Fiber> fiber;
            ///事件回调函数
            std::function<void()> cb;
        };

        /// @brief 获取事件上下文
        /// @param event 事件类型
        /// @return 返回对应事件的上下文
        EventContext &getEventContext(Event event);

        /// @brief 重置事件上下文
        /// @param ctx 待重置上下文对象
        void resetEventContext(EventContext &ctx);

        /// @brief 触发事件
        /// @details 根据事件类型调用对应上下文结构中的调度器去调度
        ///          回调协程或函数
        /// @param event 事件类型
        void triggerEvent(Event event);


        //读事件上下文
        EventContext read;
        //写事件上下文
        EventContext write;
        //事件关联句柄
        int fd=0;
        //该fd添加了哪些事件的回调函数，或者说该fd关心哪些事件
        Event events=NONE;  
        //事件的Mutex
        MutexType mutex;
    };
private:




protected:
    void tickle() override;
    void idle() override;
    bool stopping()override;
    
    void onTimerInsertedAtFront() override;
    /**
     * @brief 重置socket句柄上下文的容器大小
     * @param[in] size 容量大小
     */
    void contextResize(size_t size);
    
    bool stopping(uint64_t& timeout);
    /**
     * @brief 判断是否可以停止
     * @return 返回是否可以停止
     */
       
private:    
    //epoll文件句柄
    int m_epfd=0;
    //pipe文件句柄，fd[0]为读端,fd[1]为写端
    int m_tickleFds[2];
    //当前等待执行的IO事件数量
    std::atomic<size_t> m_pendingEventCount={0};
    //IOManager的Mutex
    RWMutexType m_mutex;
    //socket 事件上下文的容器
    std::vector<FdContext*> m_fdContexts;

};

#endif