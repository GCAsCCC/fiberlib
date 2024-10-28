#ifndef TIMER_H
#define TIMER_H

#include<memory>
#include<functional>



//前置声明
class TimerManager;

//定时器
class Timer : public std::enable_shared_from_this<Timer>
{
    friend class TimerManager;

public:
    //定时器智能指针类型
    typedef std::shared_ptr<Timer> ptr;

    //取消定时器
    bool cancel();

    //刷新设置定时器的执行时间
    bool refresh();

    
    /// @brief 重置定时器时间
    /// @param ms 定时器执行时间间隔（单位/毫秒）
    /// @param from_now 是否从当前时间开始计算
    bool reset(uint64_t ms,bool from_now);

private:
    
    /// @brief 构造函数
    /// @param ms 定时器执行间隔时间
    /// @param cb 回调函数
    /// @param recurring 是否循环
    /// @param manager 定时器管理器
    Timer(uint64_t ms,std::function<void()> cb,
            bool recurring,TimerManager* manager);

    /// @brief 构造函数
    /// @param next 执行的时间戳
    Timer(uint64_t next);

private:
    ///是否循环定时器
    bool m_recurring=false;
    ///执行周期
    uint64_t m_ms=0;
    ///精确的执行时间
    uint64_t m_next=0;
    ///回调函数
    std::function<void()> m_cb;
    ///定时管理器
    TimerManager* m_manager=nullptr;

private:
    ///定时器比较仿函数
    struct Comparator
    {   
        /// @brief 比较定时器的智能指针的大小（按执行时间m_next排序）
        /// @param lhs 定时器的智能指针
        /// @param rhs 定时器的智能指针
        bool operator()(const Timer::ptr& lhs,const Timer::ptr& rhs) const;
    };
    
};
#endif