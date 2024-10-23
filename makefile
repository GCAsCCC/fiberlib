io_test: io_test.cpp Fiber.o IOManager.o  Semaphore.o Thread.o threadPoll_fiber_scheduler.o
	g++ -o io_test -g io_test.cpp  IOManager.o threadPoll_fiber_scheduler.o Fiber.o Semaphore.o Thread.o 

threadPoll_fiber_scheduler.o: threadPoll_fiber_scheduler.cpp threadPoll_fiber_scheduler.h Comm.h Fiber.h
	g++ -g -c threadPoll_fiber_scheduler.cpp

IOManager.o: IOManager.cpp IOManager.h threadPoll_fiber_scheduler.h threadPoll_fiber_scheduler.cpp
	g++ -g -c IOManager.cpp 

Fiber.o: Fiber.h Fiber.cpp Comm.h
	g++ -g -c Fiber.cpp

Semaphore.o: Semaphore.cpp Semaphore.h
	g++ -g -c Semaphore.cpp

Thread.o: Thread.h Thread.cpp
	g++ -g -c Thread.cpp -lpthread

clean: 
	rm *.o io_test *.gch