/**
 * @brief 协程调度器
 * @details N-M协程调度器
 *          内部有一个线程池，支持协程在线程池里面进行交换
 */
#ifndef THREADPOOLFS_H
#define THREADPOOLFS_H

#include<mutex>
#include<list>
#include<atomic>
#include<vector>
#include<memory>
#include<functional>

#include"Comm.h"
#include"Thread.h"

class Fiber;



class scheduler
{
public:
    typedef std::shared_ptr<scheduler> ptr;
    typedef std::mutex MutexType;
    
    /**
     * @brief 调度器创建
     * @param threads 线程数
     * @param use_caller 是否将当前线程也作为可被调度的线程
     * @param name 调度器名称
     */
    scheduler(size_t threads=1,bool use_caller=true,const std::string &name="Scheduler");
    
    virtual ~scheduler();

    /**
     * @brief 获取调度器名称
     */
    const std::string &getName() const{return m_name;}

    /**
     * @brief 获取当前线程调度器指针
     */
    static scheduler* GetThis();

   


    /// @brief 启动调度器
    void start();

    /// @brief 停止调度器，等所有调度任务都执行完了之后再返回
    void stop();

    /// @brief 得到当前线程的调度协程
    static Fiber* GetMainFiber();
    
protected:
    /// @brief 通知协程调度器有任务了
    virtual void tickle();

    /// @brief 协程调度函数
    void run();

    /// @brief 无任务调度时执行idle协程
    virtual void idle();

    /// @brief 返回是否可以停止
    virtual bool stopping();

    /// @brief 设置当前的协程调度器
    void setThis();

    /// @brief 返回是否有空闲的线程
    /// @details 当调度协程进入idle时，idle线程+1，从idle协程返回时idle线程-1
    bool hasIdleThreads(){return m_idleThreadCount>0;};


    private:

    /**
     * @brief 调度  任务（协程）/ 函数，可指定在哪个线程上调度
     */
    struct ScheduleTask{
        std::shared_ptr<Fiber> fiber;
        std::function<void()> cb;
        int thread;
        //指定协程 ，线程
        ScheduleTask(std::shared_ptr<Fiber> f,int thr){
            fiber=f;
            thread=thr;
        }
        //指定协程指针 ，线程
        ScheduleTask(std::shared_ptr<Fiber>* f,int thr){
            fiber.swap(*f);
            thread=thr;
        }
        //指定函数 ，线程
        ScheduleTask(std::function<void()> f,int thr){
            cb=f;
            thread=thr;
        }
        //缺省
        ScheduleTask(){thread=-1;}

        void reset(){
            fiber=nullptr;
            cb=nullptr;
            thread=-1;
        }

    };

public:

    //向调度器内加入一个任务
    template <class FiberOrCb>
    void schedule(FiberOrCb fc,int thread=-1){
        bool need_tickle=false;
        {
            m_mutex.lock();
            need_tickle=scheduleNoLock(fc,thread);
            m_mutex.unlock();
        } 
        if(need_tickle){
            tickle();
        }
       
    }
    //向调度器内批量加入任务
    template <class InputtIterator>
    void schedule(InputtIterator begin,InputtIterator end){
        bool need_tickle=false;
        {
            m_mutex.lock();
            while(begin!=end){
                need_tickle=scheduleNoLock(&*begin,-1)||need_tickle;
                ++begin;
            }
            m_mutex.unlock();
        }
        if(need_tickle){
            tickle();
        }
        
    }
    /**
     * @brief 真正的加入任务队列
     * @details 若是任务队列空了，那么可能所有线程都在idle，那就tickle一下
     */
    template<class FiberOrCb>
    bool scheduleNoLock(FiberOrCb fc,int thread){
        bool need_tickle=m_tasks.empty();
        ScheduleTask task(fc,thread);
        m_tasks.push_back(task);
        return need_tickle;
    }

    private:
    ///协程调度器的名称
    std::string m_name;
    //互斥锁
    MutexType m_mutex;
    //线程池
    std::vector<std::shared_ptr<Thread> > m_threads;
    //任务队列
    std::list<ScheduleTask> m_tasks;
    //线程池的线程ID数组
    std::vector<int> m_threadIds;
    //工作线程数量
    size_t m_threadCount=0;
    //活跃线程数
    std::atomic<size_t> m_activeThreadCount = {0};
    //idle线程数
    std::atomic<size_t> m_idleThreadCount = {0};
    //是否use caller
    bool m_useCaller;
    //m_usecaller为true时，当前线程的调度协程
    std::shared_ptr<Fiber> m_rootFiber;
    //m_useCaller为true时，调度器所在的线程的id
    int m_rootThread=0;
    //是否正在停止
    bool m_stopping=false;

};


#endif