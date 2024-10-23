

#include<iostream>
#include<sys/epoll.h>
#include <fcntl.h>

#include"IOManager.h"
#include"Fiber.h"


IOManager::IOManager(size_t threads,bool use_caller,const std::string &name)
        :scheduler(threads,use_caller,name){

            //创建epoll实例
            m_epfd=epoll_create(5000);

            assert(m_epfd>0);

            //创建pipe，获取m_tickleFds[2],其中0读，1写
            int rt=pipe(m_tickleFds);
            assert(!rt);
            //注册pipe读句柄的可读事件，用于tickle调度协程，通过epoll_event.data.fd保存描述符
            epoll_event event;
            memset(&event,0,sizeof(epoll_event));
            event.events=EPOLLIN|EPOLLET;
            event.data.fd=m_tickleFds[0];

            //设置socket为非阻塞方式，配合EPOLLET边缘触发
            rt=fcntl(m_tickleFds[0],F_SETFL,O_NONBLOCK);
            assert(!rt);

            //将管道的读描述符加入epoll多路复用，如果管道可读，idle中的epoll_wait会返回
            rt=epoll_ctl(m_epfd,EPOLL_CTL_ADD,m_tickleFds[0],&event);
            assert(!rt);

            contextResize(32);
            
            //这里直接开启了scheduler，也就是说IOManager创建即可调度协程
            start();
}

/**
 * @brief 通知调度器由任务要调度
 * @details 写pipe让idle协程从epoll_wait退出，待idle协程yield之后Scheduler::run就可以
 *          调度其他任务
 */
void IOManager::tickle(){
    if(!hasIdleThreads()){
        return;
    }
    int rt=write(m_tickleFds[1],"T",1);
    assert(rt==1);
}

/**
 * @brief idle协程
 * @details 对于IO协程调度来说，应该阻塞在等待IO事件上，idle退出的时机是epoll_wait返回，
 *            对应的操作是tickle或注册的IO事件就绪。
 *调度器无任务时会阻塞在idle协程上，对IO调度器而言，idle状态应该关注两件事，一是
 *            有没有新的调度任务，对应scheduler::schedule()。
 *如果有新的调度任务，那应该立即退出idle状态，并执行对应的任务；二是关注当前注册的所有IO
 *            事件有没有触发，如果有触发，那么应该执行IO事件对应的回调函数                    
 */
void IOManager::idle(){

    std::cout<<"idle"<<std::endl;

    //一次epoll_wait最多检测256个就绪事件，如果就绪事件超过了这个数，那么会在下轮的epoll_wait继续处理

    const uint64_t MAX_EVENTS=256;
    epoll_event* events =new epoll_event[MAX_EVENTS]();
    std::shared_ptr<epoll_event> shared_events(events,[](epoll_event* ptr){
        delete[] ptr;
    });
    while (true)
    {
        if(stopping()){
            std::cout<<"name="<<getName()<<"idle stopping exit"<<std::endl;
            break;           
        }

        //阻塞在epoll_wait上，等待事件发生
        static const int MAX_TIMEOUT=5000;
        int rt=epoll_wait(m_epfd,events,MAX_EVENTS,MAX_TIMEOUT);
        if(rt<0){
            if(errno==EINTR){//系统中断，正常，继续进行下一次epoll_wait
                continue;
            }
            std::cout<<"epoll_wait("<<m_epfd<<") (rt="<<rt<<")(errno="<<errno<<")(errstr:"<<strerror(errno)<<")"<<std::endl;
        break;
        }
        std::cout<<"epoll_wait rt="<<rt<<std::endl;
        //遍历所有发生的事件，根据epoll_wait的私有指针找到对应的FdContext，进行事件处理
        for(int i=0;i<rt;++i){
            epoll_event &event=events[i];//当前发生的事件
            if(event.data.fd==m_tickleFds[0]){//只有这一个event，可使跳出epoll_wait的阻塞，yild
                //ticklefd[0]用于通知协程调度，这时只需要把管道里的内容读完即可，本轮idle
                //结束scheduler::run会重新执行协程调度
                uint8_t dummy[256];
                while(read(m_tickleFds[0],dummy,sizeof(dummy))>0);
                continue;
            }

            //通过epoll_event的私有指针获取FdContext
            FdContext* fd_ctx=(FdContext*)event.data.ptr;
            FdContext::MutexType::Lock lock(fd_ctx->mutex);

            /**
             * EPOLLERR:出错，比如写读端已经关闭的pipe
             * EPOLLHUP:套接字对端关闭
             * 出现这两种事件，应该同时触发fd的读和写事件，否则可能出现注册的事件永远执行不道的情况
             */
            if(event.events&(EPOLLERR|EPOLLHUP)){
                event.events|=(EPOLLIN|EPOLLOUT)&fd_ctx->events;

            }
            int real_events=NONE;
            if(event.events&EPOLLIN){
                real_events|=READ;
            }
            if(event.events&EPOLLOUT){
                real_events|=WRITE;
            }
            if((fd_ctx->events&real_events)==NONE){
                continue;
            }

            //剔除已经发生的事件，将剩下的事件重新加入epoll_wait,
            //如果剩下的事件为0，表示这个fd已经不需要关注了，直接从epoll中删除
            int left_events=(fd_ctx->events&~real_events);//剔除已经发生的事件
            int op=left_events?EPOLL_CTL_MOD:EPOLL_CTL_DEL;
            event.events=EPOLLET|left_events;

            int rt2=epoll_ctl(m_epfd,op,fd_ctx->fd,&event);
            if(rt2){
                std::cout<<"epoll_ctl("<<m_epfd<<","<<(EPOLL_EVENTS)op<<","<<fd_ctx->fd<<","<<(EPOLL_EVENTS)event.events<<"):"
                <<rt2<<"("<<errno<<")("<<strerror(errno)<<")"<<std::endl;
                continue;
            }

            //处理已经发生的事件，也就是让调度器调度指定的函数或协程
            if(real_events&READ){
                fd_ctx->triggerEvent(READ);
                --m_pendingEventCount;

            }
            if(real_events&WRITE){
                fd_ctx->triggerEvent(WRITE);
                --m_pendingEventCount;
            }
        }//end for


        /**
         * 一旦处理完所有的事件，idle协程yield，这样可以让调度协程(schedluler::run)重新
         * 检查是否有新的任务要调度
         * 上面的triggerEvent实际也只是把对应的fiber重新加入调度，要执行的话还要等idle协程退出
         */
        Fiber::ptr cur=Fiber::GetThis();//assert(Fiber::GetThis());
        auto raw_ptr=cur.get();
        cur.reset();

        raw_ptr->yield();
        
    }//end while(true)
}


int IOManager::addEvent(int fd,Event event,std::function<void()> cb){

    //找到fd的FdContext，如果不存在，那就分配一个
    FdContext* fd_ctx=nullptr;
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size()>fd){//能找到
        fd_ctx=m_fdContexts[fd];
        lock.unlock();
    }else{
        lock.unlock();
        RWMutexType::WriteLock lock2(m_mutex);
        contextResize(fd*1.5);
        fd_ctx=m_fdContexts[fd];
    }

    //同一个fd不允许重复添加相同的事件
    FdContext::MutexType::Lock lock2(fd_ctx->mutex);

    //将新的事件加入epoll_wait，使用epoll_event的私有指针存储FdContext的位置
    int op=fd_ctx->events?EPOLL_CTL_MOD:EPOLL_CTL_ADD;
    epoll_event epevent;
    epevent.events=EPOLLET|fd_ctx->events|event;
    epevent.data.ptr=fd_ctx;

    int rt=epoll_ctl(m_epfd,op,fd,&epevent);
    if(rt){

        return -1;
    }

    //待执行IO事件数加1
    ++m_pendingEventCount;

    //找到这个fd的event事件对应的EventContext，对其中的scheduler，cb，fiber进行赋值
    fd_ctx->events=(Event)(fd_ctx->events|event);
    FdContext::EventContext &event_ctx=fd_ctx->getEventContext(event);
    assert(!event_ctx.m_scheduler&&!event_ctx.fiber&&!event_ctx.cb);


    //赋值scheduler和回调函数，如果回调函数为空，则把当前协程当成回调执行体
    event_ctx.m_scheduler=scheduler::GetThis();
    if(cb){
        event_ctx.cb.swap(cb);
    }else{
        event_ctx.fiber=Fiber::GetThis();
        assert(event_ctx.fiber->getState()==Fiber::RUNNING);
    }
    return 0;
}

bool IOManager::delEvent(int fd,Event event){
    //找到fd对应的FdContext
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size()<=fd){
        return false;
    }
    FdContext* fd_ctx=m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(!fd_ctx->events&event){
        return false;
    }

    //清除指定的事件，表示不关心这个事件了，如果清除之后结果为0，则从epoll_wait中删除该文件描述符

    Event new_events=(Event)(fd_ctx->events&~event);
    int op = new_events? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events=EPOLLET|new_events;
    epevent.data.ptr=fd_ctx;

    int rt=epoll_ctl(m_epfd,op,fd,&epevent);

    if(rt){
        return false;
    }
    
    //待执行的事件数减1
    --m_pendingEventCount;
    //重置该fd对应的event上下文
    fd_ctx->events =new_events;
    FdContext::EventContext & event_ctx=fd_ctx->getEventContext(event);
    fd_ctx->resetEventContext(event_ctx);
    return true;
}

bool IOManager::cancelEvent(int fd,Event event){
    //找到fd对应的FdContext
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size()<=fd){
        return false;
    }
    FdContext* fd_ctx=m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(!(fd_ctx->events&event)){
        return false;
    }
    //删除事件
    Event new_events=(Event)(fd_ctx->events&~event);
    int op=new_events?EPOLL_CTL_MOD:EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events =EPOLLET|new_events;
    epevent.data.ptr=fd_ctx;

    int rt=epoll_ctl(m_epfd,op,fd,&epevent);
    if(rt){

        return false;
    }
    
    //删除之前触发一次
    fd_ctx->triggerEvent(event);
    //活跃事件数减1
    --m_pendingEventCount;
    return true;

}

bool IOManager::cancelAll(int fd){
    //找到fd对应FdContext
    RWMutexType::ReadLock lock(m_mutex);
    if((int)m_fdContexts.size()<=fd){
        return false;
    }
    FdContext* fd_ctx=m_fdContexts[fd];
    lock.unlock();

    FdContext::MutexType::Lock lock2(fd_ctx->mutex);
    if(!fd_ctx->events){
        return false;
    }

    //删除全部事件
    int op=EPOLL_CTL_DEL;
    epoll_event epevent;
    epevent.events=NONE;
    epevent.data.ptr=fd_ctx;

    int rt=epoll_ctl(m_epfd,op,fd,&epevent);
    if(rt){
        return false;
    }

    //触发全部已注册的事件
    if(fd_ctx->events&READ){
        fd_ctx->triggerEvent(READ);
        --m_pendingEventCount;
    }
    if(fd_ctx->events&WRITE){
        fd_ctx->triggerEvent(WRITE);
        --m_pendingEventCount;
    }

    assert(fd_ctx->events==0);
    return true;

}

IOManager::~IOManager(){
    stop();
    close(m_epfd);
    close(m_tickleFds[0]);
    close(m_tickleFds[1]);


    for(size_t i=0;i<m_fdContexts.size();++i){
        if(m_fdContexts[i]){
            delete m_fdContexts[i];
        }
    }

}

bool IOManager::stopping(){
    
    //对于IOManager而言，必须等所有待调度的IO事件都执行完了才可以退出
    std::cout<<"IOstopping():    "<<"m_pendingEventCount="<<m_pendingEventCount<<std::endl;
    return m_pendingEventCount==0&&scheduler::stopping();
}

void IOManager::contextResize(size_t size) {
    m_fdContexts.resize(size);

    for(size_t i = 0; i < m_fdContexts.size(); ++i) {
        if(!m_fdContexts[i]) {
            m_fdContexts[i] = new FdContext;
            m_fdContexts[i]->fd = i;
        }
    }
}

void IOManager::FdContext::triggerEvent(IOManager::Event event) {

    assert(events & event);

    events = (Event)(events & ~event);
    EventContext& ctx = getEventContext(event);
    if(ctx.cb) {
        ctx.m_scheduler->schedule(ctx.cb);
    } else {
        ctx.m_scheduler->schedule(ctx.fiber);
    }
    ctx.m_scheduler = nullptr;
    return;
}

IOManager* IOManager::GetThis() {
    return dynamic_cast<IOManager*>(scheduler::GetThis());
}

void IOManager::FdContext::resetEventContext(EventContext &ctx)
{
    ctx.m_scheduler=nullptr;
    ctx.fiber.reset();
    ctx.cb=nullptr;

}

IOManager::FdContext::EventContext &IOManager::FdContext::getEventContext(Event event)
{
   switch(event) {
        case IOManager::READ:
            return read;
        case IOManager::WRITE:
            return write;
        default:
            assert(false);
    }
    throw std::invalid_argument("getContext invalid event");

}
