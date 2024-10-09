
#include"Thread.h"



// 当前线程的指针
static thread_local Thread* t_thread = nullptr;
static thread_local std::string t_thread_name = "UNKNOW";
// thread_local变量，就是线程局部变量，意味着这个变量是线程独有的，
// 是不能与其他线程共享的。这样就可以避免资源竞争带来的多线程的问题。

// static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
// 源码这里是使用作者前面实现的log类来打印日志调试的，
// 我在调试时没有使用作者的这个log类，就只使用了std::cout来打印调试信息。

//  获取当前线程指针
Thread* Thread::GetThis() {
    return t_thread;
}
// 获取当前线程名称
const std::string& Thread::GetName() {
    return t_thread_name;
}
// 设置当前线程姓名
void Thread::SetName(const std::string& name) {
    if(name.empty()) {
        return;
    }
    if(t_thread) {
        t_thread->m_name = name;
    }
    t_thread_name = name;
}
// 构造函数
Thread::Thread(std::function<void()> cb, const std::string& name)
    :m_cb(cb)
    ,m_name(name) {
    if(name.empty()) {
        m_name = "UNKNOW";
    }
    int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);
    /*	pthread_create的用法
		int pthread_create(pthread_t *thread, 
			const pthread_attr_t *attr,
			void *(*start_routine) (void *), void *arg);
		第一个参数用于存放指向pthread_t类型的指针（指向该线程tid号的地址）
		第二个参数表示了线程的属性，一般以NULL表示默认属性
		第三个参数是一个函数指针，就是线程执行的函数。这个函数返回值为 void*，形参为 void*
		第四个参数则表示为向线程处理函数传入的参数，若不传入，可用 NULL 填充
		这里是将类的this指针传递进去
	*/
    if(rt) {
        std::cout << "pthread_create thread fail, rt=" << rt
            << " name=" << name;
        throw std::logic_error("pthread_create error");
    }
    m_semaphore.wait();
    // 消耗一个信号量，需要配合释放信号量一同使用，否则其他进程将会阻塞等待信号量
}

Thread::~Thread() {
    if(m_thread) {
        pthread_detach(m_thread);
        //线程分离状态：指定该状态，线程主动与主控线程断开关系。
        //线程一旦终止就立刻回收它占用的所有资源，而不保留终止状态。
    }
}

void Thread::join() {
    if(m_thread) {
        int rt = pthread_join(m_thread, nullptr);
        /*	当A线程调用线程B并 pthread_join() 时，
        	A线程会处于阻塞状态，直到B线程结束后，A线程才会继续执行下去。
        	当 pthread_join() 函数返回后，被调用线程才算真正意义上的结束，
        	它的内存空间也会被释放（如果被调用线程是非分离的）。
        */
        if(rt) {
            std::cout << "pthread_join thread fail, rt=" << rt
                << " name=" << m_name;
            throw std::logic_error("pthread_join error");
        }
        m_thread = 0;
    }
}

// 线程的执行体
void* Thread::run(void* arg) {
    // arg为函数体的执行参数
    Thread* thread = (Thread*)arg;
    
    t_thread = thread;
    t_thread_name = thread->m_name;
    //thread->m_id = sylar::GetThreadId();
    thread->m_id = syscall(SYS_gettid);
    pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());

    std::function<void()> cb;
    cb.swap(thread->m_cb);// 获得执行函数

    thread->m_semaphore.notify(); // 释放信号量

    cb(); // 在这里才是真正的执行
    return 0;
}

