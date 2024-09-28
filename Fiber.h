#ifndef FIBER_H
#define FIBER_H
#include<iostream>
#include<memory>
#include<functional>
#include<ucontext.h>
#include<assert.h>
#include<string.h>


class Fiber: public std::enable_shared_from_this<Fiber>
{
    public:
        typedef std::shared_ptr<Fiber> ptr;
        
        //协程状态
        enum State{
            READY,//就绪态
            RUNNING,//运行态
            TERM//终止态
        };
    private:
        //私有无参构造函数，只允许类内GetThis（）方法调用
        Fiber();
        
    public:
        /**
         * @brief 构造用户协程
         * @param cb 协程入口函数
         * @param stacksize 该协程运行时可用的栈空间大小
         * @param run_in_scheduler 该协程是否参与调度器调度，缺省为true
         */
        Fiber(std::function<void()> cb,size_t stacksize=0,bool run_in_scheduler=true);

        ~Fiber();

        /**
         * @brief 重置协程状态和入口函数以复用栈空间
         */
        void reset(std::function<void()> cb);

        /**
         * @brief 使当前协程切换到RUNNING
         * @brief 使正在运行的协程切换到READY
         */
        void resume();

        /**
         * @brief 使当前协程让出执行权
         * @details 非对称协程所有的yield都是回到调度协程那里
         */
        void yield();

        /**
         * @brief 协程ID获取
         */
        uint64_t getId() const{return m_id;}

        /**
         * @brief 获取协程当前状态
         */
        State getState() const{return m_state;}


    public:
        /**
         * @brief 设置局部变量t_Fiber的值为当前正在运行的协程
         */
        static void SetThis(Fiber* f);

        /**
         * @brief 返回当前正在执行的协程
         * @details 如果还未创建协程，就创建一个协程
         * ，改协程是当前线程的主协程，其他协程都要被该协程调度       
         */
        static Fiber::ptr GetThis();
        /**
         * @brief 获得总的协程数量
         */
        static uint64_t TotalFibers();
        /**
         * @brief 协程入口函数
         * @details 里面封装了m_cb ，主要是在m_cb后面加上了yield
         * ，调用的时候都会调用这个函数
         */
        static void MainFunc();
        /***
         * @brief 获取当前协程id
         */
        static uint64_t GetFiberId();

    private:
        /// 协程的id
        uint64_t m_id=0;
        /// 协程栈大小
        uint32_t m_stacksize=0;
        /// 协程当前状态
        State m_state=READY;
        /// 协程上下文
        ucontext_t m_ctx;
        /// 协程栈地址
        void* m_stack=nullptr;
        /// 协程的入口函数
        std::function<void()> m_cb;
        /// 协程是都参与调度器调度
        bool m_runInscheduler;

};

#endif
