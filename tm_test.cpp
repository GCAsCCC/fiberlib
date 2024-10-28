
#include"IOManager.h"
#include"util.h"
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <memory>



void tcb1(){
    std::cout<<"tcb1 now_time:"<<GetCurrentMS()<<std::endl;
}

void tcb2(){
    std::cout<<"!!-->>tcb2 now_time:"<<GetCurrentMS()<<std::endl;
}

void tcb3(){
    std::cout<<"!!-->tcb3 now_time:"<<GetCurrentMS()<<std::endl;
}

void test1() {
    
    std::weak_ptr<int> wptr;
    IOManager iom(2,false,"iomanager_1");
    //iom.addTimer(5000,tcb1,true);
    iom.addTimer(10000,tcb2,false);
    auto a=std::make_shared<int>(1);
    wptr=a;
    iom.addConditionTimer(1000,tcb3,wptr,false);
    sleep(10);
}


int main(int argc, char** argv) {
    test1();
    std::cout<<"---------------main exit----------------"<<std::endl;
    return 0;
}


