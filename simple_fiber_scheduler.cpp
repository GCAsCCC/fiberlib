/**
 * @file simple_fiber_scheduler.cpp
 * @brief 一个简单的调度器实现
 */
#include "Fiber.h"
#include <vector>
class Scheduler{
    public:
    /**
     * @brief 添加调度任务
     */
    void schedule(Fiber::ptr task){
        m_tasks.push_back(task);
    }
    /**
     * @brief 执行调度任务
     */
    void run(){
        Fiber::ptr task;
        auto it=m_tasks.begin();

        while(it!=m_tasks.end()){
            task=*it;
            m_tasks.erase(it);
            task->resume();
        }
    }

    private:
    
    std::vector<Fiber::ptr> m_tasks;

};

void test_cb(int i){
    std::cout<<"hello world"<<i<<std::endl;
}

int main(){
    //初始化主协程
    Fiber::GetThis();
    //创建调度器
    Scheduler sc;

    //添加任务
    for(int i=0;i<10;i++){
        Fiber::ptr fiber(new Fiber(std::bind(test_cb,i)));
        sc.schedule(fiber);
    }
    sc.run();
    return 0;
}