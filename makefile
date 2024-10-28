tm_test: tm_test.cpp Fiber.o IOManager.o  Semaphore.o Thread.o threadPoll_fiber_scheduler.o TimerManager.o util.o
	g++ -o tm_test -g tm_test.cpp  IOManager.o TimerManager.o Timer.o threadPoll_fiber_scheduler.o Fiber.o Semaphore.o Thread.o util.o

threadPoll_fiber_scheduler.o: threadPoll_fiber_scheduler.cpp threadPoll_fiber_scheduler.h Comm.h Fiber.h
	g++ -g -c threadPoll_fiber_scheduler.cpp

util.o: util.cpp util.h
	g++ -g -c util.cpp

Timer.o: Timer.cpp Timer.h
	g++ -g -c Timer.cpp

TimerManager.o: TimerManager.cpp TimerManager.h util.cpp util.h
	g++ -g -c TimerManager.cpp

IOManager.o: IOManager.cpp IOManager.h threadPoll_fiber_scheduler.h threadPoll_fiber_scheduler.cpp TimerManager.h TimerManager.cpp
	g++ -g -c IOManager.cpp

Fiber.o: Fiber.h Fiber.cpp Comm.h
	g++ -g -c Fiber.cpp

Semaphore.o: Semaphore.cpp Semaphore.h
	g++ -g -c Semaphore.cpp

Thread.o: Thread.h Thread.cpp
	g++ -g -c Thread.cpp -lpthread

clean: 
	rm *.o io_test *.gch tm_test test