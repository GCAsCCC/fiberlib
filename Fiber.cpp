#ifndef FIBER_CPP
#define FIBER_CPP

#include"Comm.h"
#include"Fiber.h"
#include"threadPoll_fiber_scheduler.h"

/// 协程总数
static uint64_t s_fiber_count=0;
/// 当前已用协程id号
static uint64_t s_fiber_id=0;

///当前线程正在运行的协程
static thread_local Fiber* t_fiber=nullptr;
///当前线程的主协程
static thread_local std::shared_ptr<Fiber> t_thread_fiber=nullptr;

Fiber::Fiber()
{
    
    SetThis(this);//缺省构造了第一个协程，这时运行的协程一定是这个协程，因为没有其他协程
    m_state=RUNNING;

    if(getcontext(&m_ctx))//获得当前上下文，存到m_ctx里
    {
        assert(false);
    }
    ++s_fiber_count;
    m_id=s_fiber_id++;//协程id从0开始，用完+1
    std::cout<<"Fiber() main fiber create id=="<<m_id<<std::endl;

}

Fiber::Fiber(std::function<void()> cb, size_t stacksize, bool run_in_scheduler)
:m_id(s_fiber_id++),m_cb(cb),m_runInscheduler(run_in_scheduler)
{
    ++s_fiber_count;
    m_stacksize= stacksize ? stacksize: FIBER_STACK_SIZE_DEFAULT;
    m_stack=malloc(m_stacksize);
    memset(m_stack,0,m_stacksize);

    if(getcontext(&m_ctx))
    {
        assert(false);
    }
    m_ctx.uc_link=nullptr;
    m_ctx.uc_stack.ss_size=m_stacksize;
    m_ctx.uc_stack.ss_sp=m_stack;

    makecontext(&m_ctx,&Fiber::MainFunc,0);
    std::cout<<"Fiber(...) user fiber create id=="<<m_id<<std::endl;

}

Fiber::~Fiber()
{
    --s_fiber_count;
    //m_stack不为0，使用独立栈的是子协程
    if(m_stack){
        //正在运行
     assert(m_state==TERM||m_state==READY);
    }else{
        //主协程的释放应该保证自己正在运行 且 其子协程没有任务
        assert(!m_cb);
        assert(m_state==RUNNING);

        Fiber* cur=t_fiber;
        //将当前正在运行的协程置空
        if(cur==this)
        {
            SetThis(nullptr);
        }
    }

}

void Fiber::reset(std::function<void()> cb)
{
    assert(m_stack);//当前必须有可复用的空间
    assert(m_state==TERM);//当前协程必须已经停止
    m_cb=cb;
    if(getcontext(&m_ctx)){
        assert(false);
    }
    m_ctx.uc_link=nullptr;
    m_ctx.uc_stack.ss_size=m_stacksize;
    m_ctx.uc_stack.ss_sp=m_stack;

    makecontext(&m_ctx,&Fiber::MainFunc,0);
    m_state=READY;

}

void Fiber::resume()
{
    assert(m_state!=TERM && m_state!=RUNNING );
    SetThis(this);
    m_state=RUNNING;
    /**
     * @brief swapcontext将本协程与主协程（正在运行的协程）交换
     * @brief 多线程版本要与该协程的调度协程交换（子-子 交换），不是主协程
     */


    if(m_runInscheduler){ //std::cout<<"resume():   "<<scheduler::GetMainFiber()->getId()<<"---to---"<<getId()<<std::endl; assert(&m_ctx);
        if(swapcontext(&(scheduler::GetMainFiber()->m_ctx),&m_ctx)){
            assert(false);
        }
    }else{
        if(swapcontext(&(t_thread_fiber->m_ctx),&m_ctx)){
                assert(false);
            }
    }

}

void Fiber::yield()
{
    assert(m_state==RUNNING||m_state==TERM);
    SetThis(t_thread_fiber.get());
    if(m_state!=TERM){
        m_state=READY;
    }

   
    if(m_runInscheduler){//std::cout<<"yield():   "<<getId()<<"---to---"<<scheduler::GetMainFiber()->getId()<<std::endl;
        if(swapcontext(&m_ctx,&(scheduler::GetMainFiber()->m_ctx))){
            assert(false);
        }
    }else{
        if(swapcontext(&m_ctx,&(t_thread_fiber->m_ctx))){
        assert(false);
    }
    }
    

}

void Fiber::SetThis(Fiber *f)
{
    t_fiber=f;
}

Fiber::ptr Fiber::GetThis()
{
    ///若是当前已经有了正在运行的协程便直接返回这个协程
    if(t_fiber){
        return t_fiber->shared_from_this();
    }
    ///否则创建主协程
    Fiber::ptr main_fiber(new Fiber);
    assert(t_fiber==main_fiber.get());
    t_thread_fiber=main_fiber;

    return t_fiber->shared_from_this();
}

uint64_t Fiber::TotalFibers()
{
    
    return s_fiber_count;
}

void Fiber::MainFunc()
{
    
    Fiber::ptr cur=GetThis();//get里面的shared_from_this方法会让引用计数加1
    assert(cur);

    //子协程真正的入口处
    cur->m_cb();
    cur->m_cb=nullptr;
    cur->m_state=TERM;//cur->m_cb()后该函数的协程一定执行完毕，RUNNING->READY的情况应该在m_cb里

    
    std::cout<<"thread_name: "<<Thread::GetName()<<" Fiber:"<<cur->m_id<<" is TREM"<<std::endl;
    

    auto tem_ptr =cur.get();
    cur.reset();//手动让t_fiber的引用减1
    tem_ptr->yield();//回到主协程

}

uint64_t Fiber::GetFiberId()
{
    
    return t_fiber->m_id;
}
#endif 