#include <dlfcn.h>
#include <sys/socket.h>


#include"hook.h"
#include"fd_manager.h"
#include"IOManager.h"
#include"Fiber.h"
#include <stdarg.h>
#include <fcntl.h>
#include <asm-generic/ioctls.h>


#define ETIMEOUT 1
#define HOOK_FUN(XX) \
    XX(sleep) \
    XX(usleep) \
    XX(nanosleep) \
    XX(socket) \
    XX(connect) \
    XX(accept) \
    XX(read) \
    XX(readv) \
    XX(recv) \
    XX(recvfrom) \
    XX(recvmsg) \
    XX(write) \
    XX(writev) \
    XX(send) \
    XX(sendto) \
    XX(sendmsg) \
    XX(close) \
    XX(fcntl) \
    XX(ioctl) \
    XX(getsockopt) \
    XX(setsockopt)

//线程局部变量 hook在该线程是否启用
static thread_local bool hook_enable=false;

bool is_hook_enable(){
    return hook_enable;
}
void set_hook_enable(bool flag){
    hook_enable=flag;    
}


void hook_init(){

    bool is_inited=false;
    if(is_inited==true){
        return;
    }

#define XX(name) name ## _f =(name ## _fun)dlsym(RTLD_NEXT,#name);

    HOOK_FUN(XX);

#undef XX

}

static uint64_t s_connect_timeout = -1;
struct _HookIniter {
    _HookIniter() {
        hook_init();
        s_connect_timeout = 5000;
    }
};
static _HookIniter s_hook_initer;//静态的，也就是说上来就会init，在main函数运行之前就会获取各个符号的地址并保存在全局变量中

struct timer_info {
    int cancelled = 0;
};

//省事，部分函数直接用这个模板
template<typename OriginFun, typename... Args>
static ssize_t do_io(int fd, OriginFun fun, const char* hook_fun_name,
        uint32_t event, int timeout_so, Args&&... args) {//Args&& 引用折叠
    if(!hook_enable) {//如果不启用hook，直接调用系统接口
        return fun(fd, std::forward<Args>(args)...);//forward完美转发,就是要完美保留参数的左值，右值属性
    }

    FdCtx::ptr ctx = FdManager::GetInstance()->get(fd);
    if(!ctx) {//如果fd不是socket，直接调用系统接口
        return fun(fd, std::forward<Args>(args)...);
    }

    if(ctx->isClose()) {//fd关闭，错误
        errno = EBADF;
        return -1;
    }

    if(!ctx->isSocket() || ctx->getUserNonblock()) {//用户主动设置非阻塞，那就直接非阻塞执行系统调用，没hook什么事了
        return fun(fd, std::forward<Args>(args)...);
    }

    uint64_t to = ctx->getTimeout(timeout_so);
    std::shared_ptr<timer_info> tinfo(new timer_info);

retry:
    ssize_t n = fun(fd, std::forward<Args>(args)...);
    while(n == -1 && errno == EINTR) {//忽略掉这个错误，重新执行fun
        n = fun(fd, std::forward<Args>(args)...);
    }
    if(n == -1 && errno == EAGAIN) {//EAGAIN，说明当前fd没有可读数据或可写空间
        IOManager* iom = IOManager::GetThis();
        Timer::ptr timer;
        std::weak_ptr<timer_info> winfo(tinfo);

        if(to != (uint64_t)-1) {//若超时参数有效，添加条件定时器
            timer = iom->addConditionTimer(to, [winfo, fd, iom, event]() {
                auto t = winfo.lock();
                if(!t || t->cancelled) {
                    return;
                }
                t->cancelled = ETIMEDOUT;//已经超时，设置超时标志
                iom->cancelEvent(fd, (IOManager::Event)(event));
            }, winfo);
        }

        int rt = iom->addEvent(fd, (IOManager::Event)(event));
        if(rt) {//添加事件失败，大概率不可能，-1是true，if判断非零即真
            std::cout << hook_fun_name << " addEvent("
                << fd << ", " << event << ")";
            if(timer) {
                timer->cancel();
            }
            return -1;
        } else {
            Fiber::GetThis()->yield();//yield期间，可以去执行超时的事件，或添加的事件
            if(timer) {//定时器还在，证明没有超时就可以读写了，则取消掉定时器，goto回去调用fun
                timer->cancel();
            }
            if(tinfo->cancelled) {//已经超时，返回错误码
                errno = tinfo->cancelled;
                return -1;
            }
            goto retry;
        }
    }
    
    return n;
}

extern "C"{
    //初始化置空
#define XX(name) name ## _fun name ## _f =nullptr;
    HOOK_FUN(XX);
#undef XX

unsigned int sleep(unsigned int seconds){
    if(!hook_enable){//不启用hook
        return sleep_f(seconds);//执行原系统接口
    }

    Fiber::ptr fiber=Fiber::GetThis();
    IOManager* iom=IOManager::GetThis();
    iom->addTimer(seconds*1000,std::bind((void(scheduler::*)
            (Fiber::ptr,int thread))&IOManager::schedule
            ,iom,fiber,-1));//第一个iom是this指针，fiber就是本协程，时间到了之后就会触发本协程
    fiber->yield();
    return 0;
}

//与sleep只差在参数单位

int usleep(useconds_t usec) {
    if(!hook_enable) {
        return usleep_f(usec);
    }
    Fiber::ptr fiber = Fiber::GetThis();
    IOManager* iom = IOManager::GetThis();
    iom->addTimer(usec / 1000, std::bind((void(scheduler::*)
            (Fiber::ptr, int thread))&IOManager::schedule
            ,iom, fiber, -1));
    fiber->yield();
    return 0;
}

int nanosleep(const struct timespec* req,struct timespec* rem){//由于定时器模块精度只在毫秒级，所以nanosleep的hook其实也只在毫秒级
    if(!hook_enable) {
        return nanosleep_f(req,rem);
    }
    
    int timeout_ms=req->tv_sec*1000+req->tv_nsec/1000/1000;
    Fiber::ptr fiber = Fiber::GetThis();
    IOManager* iom = IOManager::GetThis();
    iom->addTimer(timeout_ms, std::bind((void(scheduler::*)
            (Fiber::ptr, int thread))&IOManager::schedule
            ,iom, fiber, -1));
    fiber->yield();
    return 0;
}

int socket(int domain,int type,int protocol){
    if(!hook_enable){
        return socket_f(domain,type,protocol);
    }
    int fd=socket_f(domain,type,protocol);
    if(fd==-1){
        return fd;
    }
    FdManager::GetInstance()->get(fd,true);
    return fd;
}

int connect_with_timeout(int fd,const struct sockaddr* addr,socklen_t addrlen,uint64_t timeout_ms){
    if(!hook_enable){
        return connect_f(fd,addr,addrlen);
    }
    FdCtx::ptr ctx=FdManager::GetInstance()->get(fd);
    if(!ctx||ctx->isClose()){//若是fd已经没了 EBADF  sockfd is not a valid open file descriptor.
        errno=EBADF;
        return -1;
    }

    if(!ctx->isSocket()){//只处理socket fd ，其他不管
        return connect_f(fd,addr,addrlen);
    }
    if(ctx->getUserNonblock()){//如果用户自己设置的非阻塞，那就不管，否则使用协程来模拟阻塞
        return connect_f(fd,addr,addrlen);
    }


    ///fd肯定是socket fd了

    int n=connect_f(fd,addr,addrlen);

    if(n==0){//0 成功
        return 0;
    }else if(n!=-1||errno!=EINPROGRESS){//失败且不是EINPROGRESS，出错了直接返回
        return n;
    }

    IOManager* iom=IOManager::GetThis();
    Timer::ptr timer;
    std::shared_ptr<timer_info> tinfo(new timer_info);
    std::weak_ptr<timer_info> winfo(tinfo);

    if(timeout_ms!=(uint64_t)-1){//timeout有效,则添加定时器，触发读事件
        timer=iom->addConditionTimer(timeout_ms,[winfo,fd,iom](){
                auto t=winfo.lock();
                if(!t||t->cancelled){
                    return;
                }
                t->cancelled=ETIMEDOUT;
                iom->cancelEvent(fd,IOManager::WRITE);
        },winfo);
    }

    int rt=iom->addEvent(fd,IOManager::WRITE);//没有指明回调函数，默认就是本协程
    if(rt==0){//添加事件成功
        Fiber::GetThis()->yield();//yield出去，期间定时器可能触发，connect也可能成功、
        if(timer){//如果定时还没到但已经进入本协程，证明connect已经成功，则取消定时器
            timer->cancel();//cancel会触发一次定时器的回调函数，但是得是本协程结束之后才会触发，结束时winfo已被析构，条件不足，就不会做任何动作
        }
        if(tinfo->cancelled){//定时时间已经到了返回错误码TIMEOUT
            errno=tinfo->cancelled;
            return -1;
        }
    }else{//添加失败，不大可能
        if(timer){
            timer->cancel();
        }
        std::cout<< "connect addEvent(" << fd << ", WRITE) error";
    }

    int error = 0;//已经成功了，接下来检查一下fd是否有错就可以返回了
    socklen_t len=sizeof(int);
    if(-1 == getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&len)){
        return -1;
    }
    if(!error){
        return 0;
    }else{
        error =error;
        return -1;
    }
}

int connect(int sockfd,const struct sockaddr* addr,socklen_t addrlen){
    return connect_with_timeout(sockfd,addr,addrlen,s_connect_timeout);
}

int accept(int s,struct sockaddr *addr,socklen_t *addrlen){
    int fd=do_io(s,accept_f,"accept",IOManager::READ,SO_RCVTIMEO,addr,addrlen);
    if(fd>=0){
        FdManager::GetInstance()->get(fd,true);
    }
    return fd;
}


ssize_t read(int fd, void *buf, size_t count) {
    return do_io(fd, read_f, "read", IOManager::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec *iov, int iovcnt) {
    return do_io(fd, readv_f, "readv", IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    return do_io(sockfd, recv_f, "recv", IOManager::READ, SO_RCVTIMEO, buf, len, flags);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    return do_io(sockfd, recvfrom_f, "recvfrom", IOManager::READ, SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags) {
    return do_io(sockfd, recvmsg_f, "recvmsg", IOManager::READ, SO_RCVTIMEO, msg, flags);
}

ssize_t write(int fd, const void *buf, size_t count) {
    return do_io(fd, write_f, "write", IOManager::WRITE, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec *iov, int iovcnt) {
 
    return do_io(fd, writev_f, "writev", IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int s, const void *msg, size_t len, int flags) {
    return do_io(s, send_f, "send", IOManager::WRITE, SO_SNDTIMEO, msg, len, flags);
}

ssize_t sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) {
    return do_io(s, sendto_f, "sendto", IOManager::WRITE, SO_SNDTIMEO, msg, len, flags, to, tolen);
}

ssize_t sendmsg(int s, const struct msghdr *msg, int flags) {
    return do_io(s, sendmsg_f, "sendmsg", IOManager::WRITE, SO_SNDTIMEO, msg, flags);
}

int close(int fd){
    if(!hook_enable){
        return close_f(fd);
    }

    FdCtx::ptr ctx=FdManager::GetInstance()->get(fd);
    if(ctx){
        auto iom=IOManager::GetThis();
        if(iom){
            iom->cancelAll(fd);
        }
        FdManager::GetInstance()->del(fd);
    }
    return close_f(fd);
}

int fcntl(int fd,int cmd, ... /* arg */){
    va_list va;
    va_start(va,cmd);
    switch (cmd)
    {
    case F_SETFL:
        {
            int arg=va_arg(va,int);
            va_end(va);
            FdCtx::ptr ctx=FdManager::GetInstance()->get(fd);
            if(!ctx||ctx->isClose()||!ctx->isSocket()){
                return fcntl_f(fd,cmd,arg);
            }
            ctx->setUserNonblock(arg & O_NONBLOCK);
            if(ctx->getSysNonblock()){
                arg|=O_NONBLOCK;//若是socket fd不论如何都得是非阻塞
            }else{
                arg&=~O_NONBLOCK;
            }
            return fcntl_f(fd,cmd,arg);
        }
        break;
    case F_GETFL:
        {
            va_end(va);
            int arg=fcntl_f(fd,cmd);
            FdCtx::ptr ctx=FdManager::GetInstance()->get(fd);
            if(!ctx||ctx->isClose()||!ctx->isSocket()){
                return arg;
            }
            if(ctx->getSysNonblock()){
                return arg|O_NONBLOCK;
            }else{
                return arg&~O_NONBLOCK;
            }

        }
        break;
    case F_DUPFD://以下单参数
    case F_DUPFD_CLOEXEC:
    case F_SETFD:
    case F_SETOWN:
    case F_SETSIG:
    case F_SETLEASE:
    case F_NOTIFY:
#ifdef F_SETPIPE_SZ
    case F_SETPIPE_SZ:
#endif
        {
            int arg=va_arg(va,int);
            va_end(va);
            return fcntl_f(fd,cmd,arg);
        }
        break;
    case F_GETFD://以下无参数
    case F_GETOWN:
    case F_GETSIG:
    case F_GETLEASE:
#ifdef F_GETPIPE_SZ
    case F_GETPIPE_SZ:
#endif
        {
            va_end(va);
            return fcntl_f(fd,cmd);
        }
        break;
    case F_SETLK:
    case F_SETLKW:
    case F_GETLK:
        {
            struct flock* arg =va_arg(va,struct flock*);
            va_end(va);
            return fcntl_f(fd,cmd,arg);           
        }
        break;
    case F_GETOWN_EX:
    case F_SETOWN_EX:
        {
            struct f_owner_exlock* arg =va_arg(va,struct f_owner_exlock*);
            va_end(va);
            return fcntl_f(fd,cmd,arg);
        }
        break;
    default:
        va_end(va);
        return fcntl_f(fd,cmd);  
    }
}

int ioctl(int d,unsigned long int request,...){
    va_list va;
    va_start(va,request);
    void* arg = va_arg(va,void*);
    va_end(va);

    if(FIONBIO==request){
        bool user_nonblock=!!*(int*)arg;//!! 非0置都变成1,linux内核常见写法
        FdCtx::ptr ctx=FdManager::GetInstance()->get(d);
        if(!ctx||ctx->isClose()||!ctx->isSocket()){
            return ioctl_f(d,request,arg);
        }
        ctx->setUserNonblock(user_nonblock);
    }
    return ioctl_f(d,request,arg);
}

int getsockopt(int sockfd,int level,int optname,void*optval,socklen_t *optlen){
    return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd,int level,int optname,const void*optval,socklen_t optlen){
    if(!hook_enable){
        return setsockopt_f(sockfd, level, optname, optval, optlen);
    }
    if(level==SOL_SOCKET){
        if(optname==SO_RCVTIMEO||optname==SO_SNDTIMEO){
            FdCtx::ptr ctx=FdManager::GetInstance()->get(sockfd);
            if(ctx){
                const timeval* v=(const timeval*)optval;
                ctx->setTimeout(optname,v->tv_sec*1000+v->tv_usec/1000);
            }
        }
    }
    return setsockopt_f(sockfd,level,optname,optval,optlen);
}
}

