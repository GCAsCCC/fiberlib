#ifndef TIMERMANAGER_H
#define TIMERMANAGER_H


#include<set>

#include"mutex.h"
#include"Timer.h"
//定时管理器
class TimerManager{
friend class Timer;
public:
    ///读写锁类型
    typedef RWMutex RWMutexType;

    ///构造函数
    TimerManager();

    /// @brief 析构函数
    virtual ~TimerManager();

    /// @brief 添加定时器
    /// @param ms 定时器执行间隔时间
    /// @param cb 定时器回调函数
    /// @param recurring 是否循环定时器
    Timer::ptr addTimer(__uint64_t ms,std::function<void()> cb,
            bool recurring=false);

    /// @brief 添加条件定时器，条件成立定时器才有效
    /// @param ms 定时器执行间隔时间
    /// @param cb 定时器回调函数
    /// @param weak_cond 条件
    /// @param recurring 是否循环
    Timer::ptr addConditionTimer(uint64_t ms,std::function<void()> cb,
                                std::weak_ptr<void> weak_cond,
                                bool recurring=false);

    ///到最近一个定时器执行的时间间隔（毫秒）
    uint64_t getNextTimer();

    /// @brief 获取需要执行的定时器的回调函数列表
    /// @param[out] cbs 回调函数数组
    void listExpiredCb(std::vector<std::function<void()> >& cbs);

    /// @brief 是否有定时器
    bool hasTimer();
protected:

    /// @brief 当有新的定时器插入到定时器的首部，执行该函数
    virtual void onTimerInsertedAtFront()=0;

    /// @brief 将定时器添加到管理器中
    void addTimer(Timer::ptr val,RWMutexType::WriteLock& lock);

private:
    /// @brief 系统时钟是否出现了回绕（rollover）现象，即当前时间比之前记录的时间要小很多
    /// @brief 检测服务器是否发生了校时
    bool detectClockRollover(uint64_t now_ms);

private:
    ///Mutex
    RWMutexType m_mutex;
    ///定时器集合
    std::set<Timer::ptr,Timer::Comparator> m_timers;
    ///是否触发onTimerInsertedAtFront
    bool m_tickled=false;
    ///上次执行时间
    uint64_t m_previouseTime=0;
};

#endif
