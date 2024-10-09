#include"threadPoll_fiber_scheduler.h"
#include"Fiber.h"
#include"Comm.h"


///当前线程的调度器，同一调度器下的所有线程指向同一个调度器实例
static thread_local scheduler *t_scheduler =nullptr;
///当前线程的调度协程，每个线程都独有一份，包括caller线程
static thread_local Fiber* t_scheduler_fiber=nullptr;

scheduler::scheduler(size_t threads, bool use_caller, const std::string &name)
{
    assert(threads>1);

    m_useCaller=use_caller;
    m_name=name;


    if(use_caller){

        --threads;
        Fiber::GetThis();//没有主协程时，创建
        
        t_scheduler=this;//当前线程里的调度器就是this了

        /**
         * @brief 因为使用了user_caller，主线程也run一个协程调度
         */
        m_rootFiber.reset(new Fiber(std::bind(&scheduler::run,this),0,false));

        Thread::SetName(m_name);
        t_scheduler_fiber=m_rootFiber.get();
        
        m_rootThread=Thread::GetThreadId();

        m_threadIds.push_back(m_rootThread);

    }else{
        m_rootThread=-1;
    }
    m_threadCount=threads;

}

void scheduler::start()
{
    std::cout<<"scheduler is start"<<std::endl;
    
    m_mutex.lock();
    if(m_stopping){
        std::cout<<"scheduler is stopping"<<std::endl;
        m_mutex.unlock();
        return;
    }
    assert(m_threads.empty());
    m_threads.resize(m_threadCount);
    for(size_t i=0;i<m_threadCount;i++){

       
        m_threads[i].reset(new Thread(std::bind(&scheduler::run,this),m_name+"_"+std::to_string(i)));
        m_threadIds.push_back(m_threads[i]->getId());
    }
    m_mutex.unlock();
    
}


void scheduler::stop()
{
    std::cout<<"stop"<<std::endl;
    //达到了停止条件则直接retrun
    if(stopping()){
        return;
    }
    m_stopping=true;//正在停止

    //usecaller，就只能由caller线程发起stop
    if(m_useCaller){
        assert(GetThis()==this);
    }else{
        assert(GetThis()!=this);
    }

    for(size_t i=0;i<m_threadCount;i++){
        tickle();
    }

    if(m_rootFiber){
        tickle();
    }

    //在use caller情况下，调度器协程结束时，应该返回caller协程
    if(m_rootFiber){
        m_rootFiber->resume();
        std::cout<<"----m_rootFiber end";
    }

    std::vector<Thread::ptr> thrs;
    {
        m_mutex.lock();
        thrs.swap(m_threads);
        m_mutex.unlock();
    }
    for(auto &i:thrs){
        std::cout<<"join"<<i->getId()<<std::endl;
        i->join();
    }
    std::cout<<"scheduler stop succeed ----------"<<std::endl;
}

Fiber* scheduler::GetMainFiber()
{
    return t_scheduler_fiber;
}


//当前没有什么作用，IO模块会进行重写
void scheduler::tickle()
{
    
}


void scheduler::run()
{
    std::cout<<"run"<<std::endl;
    setThis();
    if(Thread::GetThreadId()!=m_rootThread){//如果不是调度器所在线程
        t_scheduler_fiber=Fiber::GetThis().get();
    }

    Fiber::ptr idle_fiber(new Fiber(std::bind(&scheduler::idle,this)));
    Fiber::ptr cb_fiber;//预创建的协程，用于执行下面的函数型任务
    
    ScheduleTask task;
    while (true)
    {
        task.reset();//清空任务属性
        bool tickle_me=false;//是否tickle其他线程进行任务调度
        {  
            m_mutex.lock();
            std::cout<<"lock"<<std::endl;
            auto it=m_tasks.begin();
            //遍历所有调度任务
            while (it!=m_tasks.end())
            {   
                if(it->thread!=-1&&it->thread!=Thread::GetThreadId()){//不是-1，则要在指定的线程上工作，如果当前线程不是指定线程，则通知其他线程去处理
                    ++it;
                    tickle_me=true;
                    continue;
                }

                assert(it->fiber||it->cb);//到这里一定找到了一个可以在该线程上执行的任务
               
                if(it->fiber){
                    assert(it->fiber->getState()==Fiber::READY);//一定是READY的任务才能进来
                }
                
                //找到需要调度的任务，不再遍历，删除任务队列的任务，break出去执行

                task=*it;
                m_tasks.erase(it);
                ++m_activeThreadCount;
                //任务已经取走了
                break;

            }
            
            std::cout<<"unlock"<<std::endl;
            m_mutex.unlock();

            //当前线程拿完一个任务后，发现任务队列还有剩余，tickle其他线程
            tickle_me|=(it!=m_tasks.end());
        }
        if(tickle_me){
            tickle();
        }

        if(task.fiber){//task有值，且是fiber

            task.fiber->resume();//task真正的执行
            m_activeThreadCount--;
            task.reset();

        }else if(task.cb){//task有值，且是函数
                if(cb_fiber){
                    cb_fiber->reset(task.cb);
                }else{
                    cb_fiber.reset(new Fiber(task.cb));
                }   
                task.reset();            
                cb_fiber->resume();
                --m_activeThreadCount;
                cb_fiber.reset();
        }else{//task无值，一定是idle线程
            if(idle_fiber->getState()==Fiber::TERM){//idle线程也结束了，调度器停止，break出大循环
                std::cout<<"idle fiber term  threadId:  "<<Thread::GetThreadId()<<std::endl;
                break;
            }
            ++m_idleThreadCount;  std::cout<<"in idle "<<Thread::GetThreadId()<<std::endl;
            idle_fiber->resume();
            --m_idleThreadCount;  std::cout<<"out idle "<<Thread::GetThreadId()<<std::endl;
        }

        
    }
    std::cout<<"scheduler::run() exit threadId: "<<Thread::GetThreadId()<<std::endl;
}

//不断yeild
void scheduler::idle()
{   
    if(m_stopping==true) return;
    Fiber::GetThis()->yield();//协程里自己yield会变成ready，若是自己没有yeild，执行完了进入了MainFunc封装的yeild就会编程TREM
}


bool scheduler::stopping()
{
    m_mutex.lock();
    //正在停止，任务队列为空，活跃线程数为0，才可以停止
    bool ret=(m_stopping&&m_tasks.empty()&&m_activeThreadCount==0);
    m_mutex.unlock();
    return ret;
    
}


void scheduler::setThis()
{
    t_scheduler=this;
}

scheduler::~scheduler()
{
    assert(m_stopping);

    if(GetThis()==this){
        t_scheduler=nullptr;
    }

}

scheduler *scheduler::GetThis()
{
    return t_scheduler;
}

