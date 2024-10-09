// 以下是函数实现
#include"Semaphore.h"

Semaphore::Semaphore(uint32_t count) {
    if(sem_init(&m_semaphore, 0, count)) {
        throw std::logic_error("sem_init error");
    }
}

/*  throw用法简单介绍
	C++通过 “throw” 关键字 抛出一条表达式来触发一个异常
	它通常作为 条件语句的一部分 或者 作为某个函数的最后一条语句，
	当throw执行时，跟在throw后面的语句将不再被执行，
	程序的控制权从throw转移到与之匹配的catch
	（catch可能是同一个函数中的局部catch，也可能位于调用链上的其他函数）。
	try {
	throw expression;
	}
	catch(case 1) {
	
	}
	catch(case 2) {
	
	}
	catch(case 3) {
	
	}
	所谓 “try”，就是 “尝试着执行一下”，如果有异常，
	则通过throw向外抛出，随后在外部通过catch捕获并处理异常。
*/

Semaphore::~Semaphore() {
    sem_destroy(&m_semaphore);
}

void Semaphore::wait() {
    if(sem_wait(&m_semaphore)) {
        throw std::logic_error("sem_wait error");
    }
}

void Semaphore::notify() {
    if(sem_post(&m_semaphore)) {
        throw std::logic_error("sem_post error");
    }
}