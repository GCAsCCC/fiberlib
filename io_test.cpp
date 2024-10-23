#include<iostream>
#include<sys/socket.h>
#include<sys/fcntl.h>
#include<arpa/inet.h>
#include<sys/epoll.h>
#include <string.h>



#include"IOManager.h"



int sock = 0;

void test_fiber() {
    std::cout<< "test_fiber sock=" << sock<<std::endl;

    //sleep(3);

    //close(sock);
    //IOManager::GetThis()->cancelAll(sock);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(sock, F_SETFL, O_NONBLOCK);

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "192.168.210.27", &addr.sin_addr.s_addr);

    if(!connect(sock, (const sockaddr*)&addr, sizeof(addr))) {
    } else if(errno == EINPROGRESS) {
         std::cout<< "add event errno=" << errno << " " << strerror(errno)<<std::endl;
        IOManager::GetThis()->addEvent(sock, IOManager::READ, [](){
            std::cout << "read callback"<<std::endl;
        });
        IOManager::GetThis()->addEvent(sock, IOManager::WRITE, [](){
            std::cout << "write callback"<<std::endl;
            //close(sock);
            //IOManager::GetThis()->cancelEvent(sock, IOManager::READ);
            close(sock);
        });
    } else {
        std::cout << "else " << errno << " " << strerror(errno)<<std::endl;
    }

}

void test1() {
   std::cout << "EPOLLIN=" << EPOLLIN
              << " EPOLLOUT=" << EPOLLOUT << std::endl;
    IOManager iom(2,false,"iomanager_1");
    iom.schedule(&test_fiber);
}



int main(int argc, char** argv) {
    test1();
    std::cout<<"---------------main exit----------------"<<std::endl;
    return 0;
}

