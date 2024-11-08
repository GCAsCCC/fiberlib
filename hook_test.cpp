#include "IOManager.h"
#include "hook.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>


#include <stdio.h>


void test_sleep(){
    IOManager iom(1);
    iom.schedule([](){
        sleep(2);
        std::cout<<"sleep 2"<<std::endl;
    });

    iom.schedule([](){
        sleep(3);
        std::cout<<"sleep 3"<<std::endl;
    });
    std::cout<<"test_sleep"<<std::endl;
}

void test_sock(){
    int sock =socket(AF_INET,SOCK_STREAM,0);

    sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;
    addr.sin_port=htons(80);
    inet_pton(AF_INET,"196.128.10.11",&addr.sin_addr.s_addr);

    std::cout<<"begin connect";
    int rt=connect(sock,(const sockaddr*)&addr,sizeof(addr));
    std::cout<<"connect rt="<<rt<<"errno="<<errno<<std::endl;
    if(rt){
        return;
    }

    const char data[]="hello\r\n\r\n";
    rt=send(sock,data,sizeof(data),0);
    std::cout<<"send rt= "<<rt<<" errno= "<<errno<<std::endl;

    if(rt<=0){
        return;
    }
    std::string buff;
    buff.resize(4096);

    rt=recv(sock,&buff[0],buff.size(),0);
    std::cout<<"recv rt= "<<rt<<" errno= "<<errno<<std::endl;

    if(rt<=0){
        return;
    }
    
    buff.resize(rt);
    std::cout<<buff;

}

int main(int argc,char** argv){
    test_sleep();
    IOManager iom;
    iom.schedule(test_sock);
    return 0;
}
