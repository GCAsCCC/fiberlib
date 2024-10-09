#include<iostream>
#include"Fiber.h"
#include"Thread.h"
#include"threadPoll_fiber_scheduler.h"


void func(int i){
    std::cout<<"task thread:  "<<Thread::GetThreadId()<<"  "<<i<<std::endl;
    
    return;
}

int main(){
    scheduler sc(5);//5个线程的协程调度器

    sc.start();
    
    for(int i=0;i<10;i++){
        
        //int t=rand()%5;
        
        sc.schedule(std::bind(func,i));
        
    }
    sc.stop();
    std::cout<<"---------------------over--------------"<<std::endl;
    return 0;
}