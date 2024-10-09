#ifndef THREAD_H
#define THREAD_H
#include<iostream>
#include<thread>
#include<memory>
#include<functional>
#include <unistd.h>
#include<sys/syscall.h>
#include"Semaphore.h"

class Fiber;
class scheduler;


//线程类
class Thread {
public:
	// 线程智能指针类型
    typedef std::shared_ptr<Thread> ptr;
	// 线程构造函数
	// cb 线程执行函数
	// name 线程名称
    Thread(std::function<void()> cb, const std::string& name);

    ~Thread();

    // 获取线程Id
    pid_t getId() const { return m_id;}
	// 获取线程姓名
    const std::string& getName() const { return m_name;}
	
    // 等待线程执行完成
    void join();


    // 得到当前正在运行的线程指针
    static Thread* GetThis();

    // 得到当前线程名称
    static const std::string& GetName();

    // 设置当前线程名称
    static void SetName(const std::string& name);

    static pid_t GetThreadId(){return syscall(SYS_gettid); };



private:
    static void* run(void* arg);
private:
    // 线程Id
    pid_t m_id = -1;
	// 线程结构句柄
    pthread_t m_thread = 0;
	// 线程执行函数体
    std::function<void()> m_cb;
	// 线程名称
    std::string m_name;
	// 信号量
    Semaphore m_semaphore;
};

#endif